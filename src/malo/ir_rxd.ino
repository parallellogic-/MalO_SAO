#include "ir_rxd.h"
#include <cstdint>
#include <algorithm>

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
      gpio_set_pulls(pin, true, false); //set pull-up on IR RxD pin
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

void SharedDecoderBuffer::end(){
  
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
  for (uint16_t iter = 0; iter < out_buff_len; iter++) {
    message[iter] = _decode_buffer[read_buf][iter];
  }
  
  message_length = out_buff_len;

  // Clear data and clear flag so the update thread knows this buffer is completely open again
  _decode_index[read_buf] = 0;
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
  uint32_t message[257];
  uint16_t message_len;
  bool is_message=get_message(message,message_len);
  if(is_message)
  {
    Serial.print("IR Message [us]: ");
    bool state=1; //38 khz present =1, no activity =0
    for(uint16_t iter=0;iter<message_len;iter++){
      Serial.printf("%d: %.1f us, ",state,message[iter]/25.0);
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
        //Serial.printf("ALPHA: %d, %d, %d\n",_generic_decoder_ptr->get_message_at(2 * i),_generic_decoder_ptr->get_message_at(2 * i+1),_generic_decoder_ptr->get_message_at(2 * i)+_generic_decoder_ptr->get_message_at(2 * i+1));
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

bool DecoderWS2812::get_message(uint8_t *message, uint16_t &message_length, uint32_t &period_cycles){
  bool is_message=get_message(_is_ping_pong,message, message_length, period_cycles);
  if(is_message) _is_ping_pong=!_is_ping_pong;
  return is_message;
}

bool DecoderWS2812::get_message(bool is_ping_pong,uint8_t *message, uint16_t &message_length, uint32_t &period_cycles){
  if(_generic_decoder_ptr->get_ping_pong()==is_ping_pong) return false; //if asking for a message from a buffer that has already been read, then do nothing
  //found a message, now decode it...
  const uint16_t max_length=message_length;//max number of characters that can be written into output buffer
  period_cycles=_get_median_signal_period();
  //period_us=period_cycles/25;
  const uint32_t generic_message_length=(_generic_decoder_ptr->get_message_length()/2)*2;//number of 1/0 pairs
  uint8_t decoded_byte=0;//the latest byte that is being decoded (for placement into _decode_buffer)
  uint8_t bit_decode_count=0;//number of bits in the current byte that have been decoded
  uint16_t generic_index=0;//position within generic array to decod from
  uint16_t out_index=0;
  while(generic_index<generic_message_length && out_index<(max_length-1))
  {
    //Serial.printf("EPSILON: %d, %d, %d, %d\n",period_cycles,period_cycles/25,generic_message_length,out_index);
    uint32_t numerator=0;
    uint32_t denominator=0;
    while(generic_index<generic_message_length)
    {
      numerator+=_generic_decoder_ptr->get_message_at(generic_index);//legnth of 1's
      denominator+=_generic_decoder_ptr->get_message_at(generic_index);
      denominator+=_generic_decoder_ptr->get_message_at(generic_index+1);//length of 1 and 0s
      generic_index+=2;
      if( generic_index >= generic_message_length || (denominator+_generic_decoder_ptr->get_message_at(generic_index)+_generic_decoder_ptr->get_message_at(generic_index+1)) > (3*period_cycles/2) ) break;
    }
    bool decoded_bit=numerator>(denominator/2);//if 1 for more than half the time, consider this a 1, else 0
    decoded_byte=(decoded_byte<<1) | decoded_bit;
    bit_decode_count++;
    if(bit_decode_count==8)
    {
      message[out_index]=decoded_byte;
      decoded_byte=0;
      bit_decode_count=0;
      out_index++;
    }
  }
  message_length=out_index;
  
  return true;
}

void DecoderWS2812::debug(){
  uint16_t message_length=257;
  uint8_t message[258];
  uint32_t period_cycles;
  bool is_message=get_message(message,message_length,period_cycles);
  if(is_message)
  {
    //message[message_length]='\0';
    Serial.printf("IR WS2812 decode message [len: %d, period_us: %.1f, kHz: %.2f]: ",message_length,period_cycles/25.0,25'000.0/period_cycles);
    for(uint16_t iter=0;iter<message_length;iter++) Serial.printf("0x%02X ",message[iter]);
    Serial.print("\n");
  }else{
    Serial.print("IR WS2812 decode message: No Message\n");
  }
}

//error-correcting code decoder...
