#include "ir_rxd.h"
#include <cstdint>
#include <algorithm>
#include "RS-FEC.h"

// ---- SharedDecoderBuffer ----

SharedDecoderBuffer::SharedDecoderBuffer(uint8_t rxd_first_pin,uint8_t rxd_pin_count)
  : _rxd_first_pin(rxd_first_pin), _rxd_pin_count(rxd_pin_count){}

void SharedDecoderBuffer::begin(PIOProgramManager &pio_program_manager){
  //pio_set_gpio_base(_pio, 16);//need to use >pin 32 for this pio

  PIO pio=pio_program_manager.get_pio();
  int sm=pio_program_manager.allocate_sm();
  int sm_offset=pio_program_manager.get_offset();
  //if(_sm_offset<0) _sm_offset=pio_add_program(pio1, &logic_analyzer_program);

  //_sm=pio_claim_unused_sm(pio, true); 
  for(uint8_t pin=_rxd_first_pin;pin<(_rxd_first_pin+_rxd_pin_count);pin++)
  {//init the pins
    pio_gpio_init(pio, pin);
    gpio_set_input_enabled(pin, true);
    gpio_disable_pulls(pin);
    if(pin==IR_RXD_PIN)
    {
//      gpio_set_pulls(pin, true, false); //set pull-up on IR RxD pin - device just idles low until there's activity
      gpio_set_inover(pin, GPIO_OVERRIDE_INVERT);//periphreal idles high, so set to '0' when no 38 khz 940 nm light, set to '1' when 38 khz is present
    }
  }

  // 4. Configure the PIO State Machine to listen
  pio_sm_config c = logic_analyzer_program_get_default_config(sm_offset);
  
  // Set the IN pins to start at our PWM pin
  sm_config_set_in_pins(&c, _rxd_first_pin);
  sm_config_set_in_pin_count(&c, _rxd_pin_count);

  // RP2350 requires the FIFO to be joined to handle high-speed bursts
  sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_RX);

  // Initialize and start the state machine
  pio_sm_init(pio, sm, sm_offset, &c);
  pio_sm_set_enabled(pio, sm, true);

  // 4. DMA Setup
  _dma_chan = dma_claim_unused_channel(true);
  dma_channel_config dma_c = dma_channel_get_default_config(_dma_chan);
  channel_config_set_transfer_data_size(&dma_c, DMA_SIZE_32);
  channel_config_set_read_increment(&dma_c, false);
  channel_config_set_write_increment(&dma_c, true);
  channel_config_set_ring(&dma_c, true, __builtin_ctz(sizeof(_capture_buffer)));
  channel_config_set_dreq(&dma_c, pio_get_dreq(pio, sm, false));

  dma_channel_configure(_dma_chan, &dma_c, _capture_buffer, &pio->rxf[sm], 0xFFFFFFFF, true);
}

void SharedDecoderBuffer::end() {
  // 1. Stop and disable the DMA channel to prevent memory corruption
  dma_channel_abort(_dma_chan);
  dma_channel_unclaim(_dma_chan);

  // 2. Fetch active PIO hardware context from your program manager
  /*PIO pio = pio_program_manager.get_pio();
  int sm = pio_program_manager.get_active_sm(); // Assumes get_active_sm() or keeping _sm as a class member

  // 3. Stop and reset the PIO State Machine
  pio_sm_set_enabled(pio, sm, false);
  pio_sm_clear_fifos(pio, sm);
  pio_sm_unclaim(pio, sm);*/ // Returns the state machine back to the manager pool

  // 4. Reset GPIO overrides and return pins to safe default states
  for (uint8_t pin = _rxd_first_pin; pin < (_rxd_first_pin + _rxd_pin_count); pin++) {
    gpio_set_inover(pin, GPIO_OVERRIDE_NORMAL); // Remove the IR invert override
    gpio_disable_pulls(pin);                    // Reset pull-ups/pull-downs
    gpio_set_input_enabled(pin, false);         // Disable input buffer
    
    // De-assign pin from PIO back to default general purpose I/O (Software control)
    gpio_set_function(pin, GPIO_FUNC_SIO); 
  }
}

//uint16_t SharedDecoderBuffer::get_buffer_length(){ return sizeof(_capture_buffer)/sizeof(_capture_buffer[0]); }

//get the last address fully written to by DMA (write is complete)
uint16_t SharedDecoderBuffer::get_ring_buffer_index(bool is_step_back)
{
  // 1. Get the memory address the DMA is currently writing to
  uintptr_t current_address = (uintptr_t)dma_channel_hw_addr(_dma_chan)->write_addr;//next address the DMA will write to
  
  // 2. Subtract the starting memory address of your capture buffer
  uintptr_t byte_offset = current_address - (uintptr_t)_capture_buffer;
  
  // 3. Divide by the size of each element (sizeof(uint32_t) = 4 bytes) to convert bytes to element index
  uint16_t element_index = byte_offset / sizeof(_capture_buffer[0]);
  
  if(!is_step_back) return element_index;

  if(element_index==0) return sizeof(_capture_buffer)/sizeof(_capture_buffer[0])-1;//return the address of where the last full data was written
  else return --element_index;
}

void SharedDecoderBuffer::get_buffer_at(uint8_t pin_index,uint16_t current_index,uint32_t &cycles,bool &value)
{
  cycles=(1<<(32-LOGIC_ANALYZER_PIN_COUNT)) - (_capture_buffer[current_index]>>LOGIC_ANALYZER_PIN_COUNT);
  value=(_capture_buffer[current_index] >> pin_index) & 0x01;
}

bool SharedDecoderBuffer::is_activity(uint8_t pin_index, uint32_t timeout_us)
{
  const uint16_t start_index = get_ring_buffer_index(true); // Remember where we started
  uint16_t current_index = start_index;
  const uint16_t buffer_length = sizeof(_capture_buffer) / sizeof(_capture_buffer[0]);

  // Read the initial state at the current pointer
  bool static_state = (_capture_buffer[current_index] >> pin_index) & 0x01;
  uint32_t running_cycle_count = 0;
  uint32_t done_cycle_count = timeout_us * 25; // 25 MHz = 25 ticks per us

  do {
    // 1. Process the current sample first
    bool current_state;// = (_capture_buffer[current_index] >> pin_index) & 0x01;
    uint32_t cycle_count;
    get_buffer_at(pin_index,current_index,cycle_count,current_state);
    //Serial.printf("current_state: %d, static_state: %d, running_cycle_count: %d, done_cycle_count: %d, current_index: %d, start_index: %d\n", current_state,static_state, running_cycle_count,done_cycle_count,current_index,start_index);
    
    // Found a 0->1 or 1->0 transition
    if (current_state ^ static_state) return true; 

    // 2. Accumulate the clock cycles spent in this state
    //running_cycle_count += (_capture_buffer[current_index] >> LOGIC_ANALYZER_PIN_COUNT);
    running_cycle_count += cycle_count;//(1<<(32-LOGIC_ANALYZER_PIN_COUNT)) - (_capture_buffer[current_index]>>LOGIC_ANALYZER_PIN_COUNT);//NOTE: it's a count-down timer, need to invert to use values meaningfully
    
    if (running_cycle_count > done_cycle_count) return false;

    // 3. Step backward safely in the ring buffer
    if (current_index == 0) current_index = buffer_length - 1;
    else current_index--;

  } while (current_index != start_index); // Stop if we've looped through the entire buffer

  return false; 
}


// ---- generic IR decoder ----

DecoderGeneric::DecoderGeneric(uint8_t rxd_pin_index,int8_t pwm_activity_pin,uint32_t timeout_us)
  : _rxd_pin_index(rxd_pin_index), _pwm_activity_pin(pwm_activity_pin), _timeout_us(timeout_us){}

void DecoderGeneric::begin(SharedDecoderBuffer* buffer){
  _buffer_ptr=buffer;
  if(_pwm_activity_pin>=0 && (gpio_get_dir(_pwm_activity_pin) == GPIO_IN))
  {//if activity indictor pins has NOT been claimed by application user, and it's a valid input, then route it as a PWM output
    // 1. Tell the GPIO mux to route the PWM peripheral to this pin
    gpio_set_function(_pwm_activity_pin, GPIO_FUNC_PWM);

    // 2. Find out which PWM slice and channel are connected to the pin
    uint slice_num = pwm_gpio_to_slice_num(_pwm_activity_pin);
    uint channel = pwm_gpio_to_channel(_pwm_activity_pin);

    // 3. Set the period/wrap value (e.g., 65535 for 16-bit resolution)
    pwm_set_wrap(slice_num, 65535);

    // 4. Set the duty cycle level to 0 (0% duty cycle)
    set_activity(0);

    // 5. Start the PWM slice hardware running
    pwm_set_enabled(slice_num, true);
  }
}

void DecoderGeneric::update()
{
  set_activity(_buffer_ptr->is_activity(_rxd_pin_index, 34'000));

  const uint16_t dma_index = _buffer_ptr->get_ring_buffer_index(false); 
  const uint16_t buffer_length = _buffer_ptr->get_buffer_length(); 

  // 20 ms timeout expressed in 25 MHz system clock ticks
  const uint32_t TIMEOUT_TICKS = _timeout_us*25;//20'000 * 25; 

  bool current_state;
  uint32_t cycle_count;

  while (_ring_buffer_index != dma_index)
  {
    _buffer_ptr->get_buffer_at(_rxd_pin_index, _ring_buffer_index, cycle_count, current_state);
    
    // Accumulate the time slices moving forward
    uint8_t write_buf = _is_ping_pong ? 1 : 0;

    switch (_state)
    {
      case STATE_LOOKING_FOR_START:
        if (current_state == true) {
          _state = STATE_MEASURING_HIGH;
          _running_cycle_count = 0; 
        }
        break;

      case STATE_MEASURING_HIGH:
        // 1. Check if the signal actually transitioned (1 -> 0)
        if (current_state == false) {
          
          // Check if this HIGH state lasted so long that it constitutes a timeout/gap boundary
          if (_running_cycle_count >= TIMEOUT_TICKS) {
            // Log the trailing timeout duration before swapping buffers
            if (_decode_index[write_buf] < get_buffer_length()) {
              _decode_buffer[write_buf][_decode_index[write_buf]] = _running_cycle_count;// / 25;
              _decode_index[write_buf]++;
            }
            if (_decode_index[write_buf] > 0) {
              _is_read_ready[write_buf] = true; 
              _is_ping_pong = !_is_ping_pong; 
              write_buf = _is_ping_pong ? 1 : 0;
              _decode_index[write_buf] = 0; 
            }
            // Pivot cleanly: Since current_state is 0, the next state we measure is LOW
            _state = STATE_MEASURING_LOW;
          } 
          else {
            // Natural packet pulse boundary
            if (_decode_index[write_buf] < get_buffer_length()) {
              _decode_buffer[write_buf][_decode_index[write_buf]] = _running_cycle_count;// / 25;
              _decode_index[write_buf]++;
            }
            _state = STATE_MEASURING_LOW;
          }
          _running_cycle_count = 0; 
        }
        // 2. Safeguard: Prevent indefinite lockup if an edge never shows up at all
        else if (_running_cycle_count >= TIMEOUT_TICKS) {
          if (_decode_index[write_buf] > 0) {
            _is_read_ready[write_buf] = true; 
            _is_ping_pong = !_is_ping_pong; 
            write_buf = _is_ping_pong ? 1 : 0;
            _decode_index[write_buf] = 0; 
          }
          _state = STATE_LOOKING_FOR_START;
          _running_cycle_count = 0;
        }
        break;

      case STATE_MEASURING_LOW:
        // 1. Check if the signal actually transitioned (0 -> 1)
        if (current_state == true) {
          
          // Check if this LOW gap lasted so long that it marks the end of an entire transmission string
          if (_running_cycle_count >= TIMEOUT_TICKS) {
            // Log the trailing gap duration before locking out the buffer segment
            if (_decode_index[write_buf] < get_buffer_length()) {
              _decode_buffer[write_buf][_decode_index[write_buf]] = _running_cycle_count;// / 25;
              _decode_index[write_buf]++;
            }
            if (_decode_index[write_buf] > 0) {
              _is_read_ready[write_buf] = true; 
              _is_ping_pong = !_is_ping_pong; 
              write_buf = _is_ping_pong ? 1 : 0;
              _decode_index[write_buf] = 0; 
            }
            // Pivot cleanly: Since current_state is 1, a brand new message has begun!
            _state = STATE_MEASURING_HIGH;
          } 
          else {
            // Natural packet space boundary
            if (_decode_index[write_buf] < get_buffer_length()) {
              _decode_buffer[write_buf][_decode_index[write_buf]] = _running_cycle_count;// / 25;
              _decode_index[write_buf]++;
            }
            _state = STATE_MEASURING_HIGH;
          }
          _running_cycle_count = 0; 
        }
        // 2. Safeguard: Prevent indefinite lockup if the line goes dead silent indefinitely
        else if (_running_cycle_count >= TIMEOUT_TICKS) {
          if (_decode_index[write_buf] > 0) {
            _is_read_ready[write_buf] = true; 
            _is_ping_pong = !_is_ping_pong; 
            write_buf = _is_ping_pong ? 1 : 0;
            _decode_index[write_buf] = 0; 
          }
          _state = STATE_LOOKING_FOR_START;
          _running_cycle_count = 0;
        }
        break;
    }

    _ring_buffer_index++;
    if (_ring_buffer_index >= buffer_length) {
      _ring_buffer_index = 0;
    }
    _running_cycle_count += cycle_count;
  }
}



bool DecoderGeneric::get_message(uint32_t *message, uint16_t &message_length)
{
  // The reading buffer is always the OPPOSITE of the active writing buffer
  uint8_t read_buf = !_is_ping_pong ? 1 : 0;

  // Check if a completed message packet is actually flagged and waiting for us
  if (!_is_read_ready[read_buf]) return false;

  uint16_t out_buff_len = _decode_index[read_buf];
  if (out_buff_len == 0) {
    _is_read_ready[read_buf] = false; // Guard safety reset
    return false;
  }

  if (out_buff_len > get_buffer_length()) out_buff_len = get_buffer_length();

  // Copy snapshot contents from the inactive ping-pong slot
  /*for (uint16_t iter = 0; iter < out_buff_len; iter++) {
    message[iter] = _decode_buffer[read_buf][iter];
  }*/
  memcpy(message, _decode_buffer[read_buf], out_buff_len);
  
  message_length = out_buff_len;

  // Clear data and clear flag so the update thread knows this buffer is completely open again
  //_decode_index[read_buf] = 0;
  _is_read_ready[read_buf] = false; 

  return true;
}


//show acitivty on led indictor, if any
void DecoderGeneric::set_activity(bool is_activity)
{
  if(_pwm_activity_pin<0) return; //invalid input, nothin to set
  uint slice_num = pwm_gpio_to_slice_num(_pwm_activity_pin);
  uint channel = pwm_gpio_to_channel(_pwm_activity_pin);
  pwm_set_chan_level(slice_num, channel, is_activity?DECODER_ACTIVITY_BRIGHTNESS:0);
}

void DecoderGeneric::debug()
{
  Serial.printf("IR RxD Activity: is_activity: %d, activity_pin_mode: %d, activity_pin: %d, _buffer_ptr.index: %d, _decode_index: %d, %d, _state: %d\n",_buffer_ptr->is_activity(_rxd_pin_index,20'000),gpio_get_function(_pwm_activity_pin),_pwm_activity_pin,_buffer_ptr->get_ring_buffer_index(true),_decode_index[0],_decode_index[1],_state);
  uint32_t message[DECODER_MAX_WS2812_MESSAGE_LENGTH];
  uint16_t message_len;
  bool is_message=get_message(message,message_len);
  if(is_message)
  {
    Serial.printf("IR Message [us, state count: %d]: ",message_len);
    bool state=1; //38 khz present =1, no activity =0
    for(uint16_t iter=0;iter<message_len;iter++){
//      Serial.printf("%d: %.1f us, ",state,message[iter]/25.0);
      state=!state;
    }
    Serial.println();
  }else{
    Serial.print("IR Message: No Message\n");
  }
  //while(1) tight_loop_contents();//halt on first message received
}


// ---- DecoderWS2812 ----

DecoderWS2812::DecoderWS2812(){}

void DecoderWS2812::begin(DecoderGeneric* generic_decoder){
  _generic_decoder_ptr=generic_decoder;
}

uint32_t DecoderWS2812::_get_median_signal_period() {
    uint16_t valid_length = _generic_decoder_ptr->get_message_length();
    // Each period requires a pair of '1' and '0'
    //Serial.printf("BETA: %d\n",valid_length);
    size_t total_pairs = valid_length / 2;
    if (total_pairs == 0) {
        return 0;
    }

    // Defensive check to prevent stack overflow if valid_length is malformed
    if (total_pairs > (DECODER_MAX_GENERIC_MESSAGE_LENGTH / 2)) {
        total_pairs = DECODER_MAX_GENERIC_MESSAGE_LENGTH / 2;
    }

    // Allocate a fixed-size buffer on the stack (zero dynamic allocation)
    uint32_t periods[DECODER_MAX_GENERIC_MESSAGE_LENGTH / 2];

    // Step 1: Compute periods into the stack buffer
    for (size_t i = 0; i < total_pairs; ++i) {
        periods[i] = _generic_decoder_ptr->get_message_at(2 * i) + _generic_decoder_ptr->get_message_at((2 * i) + 1);
    }

    // Step 2: Find the median within the stack buffer using O(N) selection
    size_t mid = total_pairs / 2;
    
    if (total_pairs % 2 != 0) {
        // Odd number of pairs: median is at the middle index
        std::nth_element(periods, periods + mid, periods + total_pairs);
        //Serial.printf("DELTA: %d\n",periods[mid]);
        return periods[mid];
    } else {
        // Even number of pairs: average the two middle elements
        std::nth_element(periods, periods + mid, periods + total_pairs);
        uint32_t high_mid = periods[mid];
        
        // Find the largest element in the lower partition [0, mid)
        auto low_mid_ptr = std::max_element(periods, periods + mid);
        uint32_t low_mid = *low_mid_ptr;
        
        //Serial.printf("DELTA2: %d\n",(low_mid + high_mid) / 2);
        return (low_mid + high_mid) / 2;
    }
}

float DecoderWS2812::_get_exact_frequency(float base_frequency_hz,uint8_t period) {
    float sys_clk_hz = (float)clock_get_hz(clk_sys);
    
    float period_f=period+1.0;

    // 1. Calculate the ideal floating-point divider
    // Wrap is 255, meaning 256 total steps (TOP + 1)
    float dynamic_div = sys_clk_hz / (base_frequency_hz * period_f);
    
    // 2. Mimic the Pico SDK float-to-fixed conversion (Truncation to 8.4 format)
    // The SDK multiplies by 16 and casts to an integer to drop the lower bits
    uint32_t fixed_point_reg = (uint32_t)(dynamic_div * 16.0f);
    
    // 3. Convert the hardware register value back to the actual floating-point divider used
    float hardware_div = (float)fixed_point_reg / 16.0f;
    
    // 4. Calculate the true physical frequency output by the PWM hardware
    float exact_frequency_hz = sys_clk_hz / (hardware_div * 256.0f);
    
    return exact_frequency_hz;
}

bool DecoderWS2812::get_message(char *username,char *message){//, uint16_t &message_length){
  bool is_message=get_message(_is_ping_pong,username,message);//, message_length);
  if(is_message) _is_ping_pong=!_is_ping_pong;
  return is_message;
}

void DecoderWS2812::_decompress78(const uint8_t* in_arr, char* out_arr)
{
  uint64_t in_val = 0;

  // 1. Reconstruct the 64-bit integer from the 7 packed bytes
  for (uint8_t iter = 0; iter < 7; iter++)
  {
    in_val = (in_val << 8) | in_arr[iter];
  }

  // 2. Extract the 8 original characters (7 bits each) from top to bottom
  uint8_t out_index = 0;
  for (int8_t shift = 49; shift >= 0; shift -= 7)
  {
    out_arr[out_index] = (in_val >> shift) & 0x7F;
    out_index++;
  }
  
  // Optional: Null-terminate if out_arr is treated as a standard C-string
  // out_arr[out_index] = '\0'; 
}

bool DecoderWS2812::get_message(bool is_ping_pong,char *username,char *message){//, uint16_t &message_length){
  if(_generic_decoder_ptr->get_ping_pong()==is_ping_pong) return false; //if asking for a message from a buffer that has already been read, then do nothing
  //found a message, now decode it...
  //const uint16_t max_length=message_length;//max number of characters that can be written into output buffer
  const uint32_t generic_message_length=(_generic_decoder_ptr->get_message_length()/2)*2;//number of 1/0 pairs
  uint8_t decoded_byte=0;//the latest byte that is being decoded (for placement into _decode_buffer)
  uint16_t generic_index=0;//position within generic array to decod from
  uint16_t out_index=0;
  float exact_carrier_hz=_get_exact_frequency(38'000,255);
  uint8_t raw_message[DECODER_MAX_WS2812_MESSAGE_LENGTH+RS_ECC_LENGTH]={};
  while(generic_index<generic_message_length && out_index<(sizeof(raw_message)-1))
  {
    if(_generic_decoder_ptr->get_message_at(generic_index)>_generic_decoder_ptr->get_message_at(generic_index+1)) generic_index++;//soemthing amiss with the GenericDecover that puts the inter-message dwell as the first decoded duration, patching that here for now... TOOD
    uint32_t numerator=_generic_decoder_ptr->get_message_at(generic_index);//legnth of 1's (38 khz)
    uint32_t denominator=_generic_decoder_ptr->get_message_at(generic_index+1);//length of 0 (no activity)
    //Serial.printf("plumbob %d, %u, %u\n",generic_index,numerator,denominator);
    generic_index+=2;
      
    uint16_t numerator_byte=(uint8_t)((numerator+25'000'000.0f/exact_carrier_hz/2.0f)*exact_carrier_hz/25'000'000.0f)/1;//min(0x0003,(uint8_t)((numerator+25'000'000.0f/exact_carrier_hz/2.0f)*exact_carrier_hz/25'000'000.0f - 12)/1);
    uint16_t denominator_byte=(uint8_t)((denominator+numerator+25'000'000.0f/exact_carrier_hz/2.0f)*exact_carrier_hz/25'000'000.0f - 16-24)/1;//,false);
    if(denominator_byte>0x01FF || numerator_byte>70) continue;//bad byte read
    denominator_byte=min(0x00FF,denominator_byte);
    uint8_t decoded_byte=denominator_byte;
        raw_message[out_index]=decoded_byte;
        //Serial.printf("Beach %d %02X, %d<%d %d<%d\n",out_index,raw_message[out_index],generic_index,generic_message_length,out_index, max_length-1 );
        decoded_byte=0;
        out_index++;
  }
  RS::ReedSolomon<DECODER_MAX_WS2812_MESSAGE_LENGTH, RS_ECC_LENGTH> rs;
  uint8_t decoded[DECODER_MAX_WS2812_MESSAGE_LENGTH]={};
  bool is_error=rs.Decode(raw_message, decoded);
  //Serial.printf("Bogus: %d\n",is_error);
  if(is_error) return false;
  /*for(uint8_t iter=0;iter<sizeof(decoded)/sizeof(decoded[0]);iter++)
  {
    if(iter>0 && iter%16==0) Serial.printf("\n");
    Serial.printf("%02X ",decoded[iter]);
  }*/

  //message_length=min(DECODER_MAX_WS2812_MESSAGE_LENGTH,decoded[0]);
  for(uint8_t iter=0;(iter*8)<USERNAME_MAX_LENGTH;iter++) _decompress78(&decoded[1+iter*7],&username[iter*8]);
  for(uint8_t iter=0;(iter*8)<MESSAGE_MAX_LENGTH;iter++) _decompress78(&decoded[1+USERNAME_MAX_LENGTH*7/8+iter*7],&message[iter*8]);

  //enforce null termination on string
  username[USERNAME_MAX_LENGTH-1]='\0';
  message[MESSAGE_MAX_LENGTH-1]='\0';

  return true; //true on success
}

void DecoderWS2812::debug(){
  //uint16_t message_length=DECODER_MAX_WS2812_MESSAGE_LENGTH+RS_ECC_LENGTH;
  char username[USERNAME_MAX_LENGTH];
  char message[MESSAGE_MAX_LENGTH];
  bool is_message=get_message(username,message);//,message_length);
  if(is_message)
  {
    //message[message_length]='\0';
    Serial.printf("IR WS2812 decode message, username: %s, message: %s\n",username,message);
    //int error_count=0;
    /*Serial.printf("username:\n");
    for(uint16_t iter=0;iter<sizeof(username);iter++)
    {
      if(iter%16==0 && iter>0) Serial.print("\n");
      Serial.printf("0x%02X ",username[iter]); //if(iter!=username[iter]) error_count++;
      //Serial.printf("0x%02X%c",message[iter],(255-iter)==message[iter]?' ':'*'); if((255-iter)!=message[iter]) error_count++;
    }
    Serial.printf("\nmessage:\n",message_length,username,message);
    for(uint16_t iter=0;iter<sizeof(message);iter++)
    {
      if(iter%16==0 && iter>0) Serial.print("\n");
      Serial.printf("0x%02X ",message[iter]); //if(iter!=message[iter]) error_count++;
      //Serial.printf("0x%02X%c",message[iter],(255-iter)==message[iter]?' ':'*'); if((255-iter)!=message[iter]) error_count++;
    }
    //Serial.printf("\nerror_count: %d, %.2f%%\n",error_count,error_count*100.0f/message_length);
    Serial.printf("\n");*/
  }else{
    Serial.print("IR WS2812 decode message: No Message\n");
  }
}

//error-correcting code decoder...
