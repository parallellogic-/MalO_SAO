#include "ir_rxd.h"

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

uint16_t SharedDecoderBuffer::get_buffer_index()
{
  // 1. Get the memory address the DMA is currently writing to
  uintptr_t current_address = (uintptr_t)dma_channel_hw_addr(_dma_chan)->write_addr;
  
  // 2. Subtract the starting memory address of your capture buffer
  uintptr_t byte_offset = current_address - (uintptr_t)_capture_buffer;
  
  // 3. Divide by the size of each element (sizeof(uint32_t) = 4 bytes) to convert bytes to element index
  uint16_t element_index = byte_offset / sizeof(_capture_buffer[0]);
  
  return element_index;
}


bool SharedDecoderBuffer::is_activity(uint8_t pin_index, uint32_t timeout_us)
{
  uint16_t start_index = get_buffer_index(); // Remember where we started
  uint16_t current_index = start_index;
  const uint16_t buffer_length = sizeof(_capture_buffer) / sizeof(_capture_buffer[0]);

  // Read the initial state at the current pointer
  bool static_state = (_capture_buffer[current_index] >> pin_index) & 0x01;
  uint32_t running_cycle_count = 0;
  uint32_t done_cycle_count = timeout_us * 25; // 25 MHz = 25 ticks per us

  do {
    // 1. Process the current sample first
    bool current_state = (_capture_buffer[current_index] >> pin_index) & 0x01;
    
    // Found a 0->1 or 1->0 transition
    if (current_state ^ static_state) return true; 

    // 2. Accumulate the clock cycles spent in this state, max: (LOGIC_ANALYZER_PIN_COUNT-1)
    running_cycle_count += (_capture_buffer[current_index] >> pin_index);
    
    if (running_cycle_count > done_cycle_count) return false;

    // 3. Step backward safely in the ring buffer
    if (current_index == 0) current_index = buffer_length - 1;
    else current_index--;

  } while (current_index != start_index); // Stop if we've looped through the entire buffer

  return false; 
}


// ---- DecoderWS2812 ----

DecoderWS2812::DecoderWS2812(uint8_t rxd_pin_index,int8_t pwm_activity_pin)
  :  _rxd_pin_index(rxd_pin_index), _pwm_activity_pin(pwm_activity_pin){}

void DecoderWS2812::begin(SharedDecoderBuffer* buffer){
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

void DecoderWS2812::update(){
  set_activity(_buffer_ptr->is_activity(_rxd_pin_index,20'000));
  if(_period==0) _period=get_period(); //if period unset, then determine period
  if(_period==0) return;//if still unset, then no action to perform
  //update state machine...

}

/*uint32_t DecoderWS2812::get_byte()
{
  //look through backward from the current pointer at the most recent 8x 0->1 transitions. store the median of that as the period (but don't push live until start of message found,otherwise just looking at mid-message jibberish)
  //then keep going back until a gap of >=40*period is found(or wrap around back to current pointer)

  uint16_t latest_buffer_index=_buffer_ptr->get_buffer_index();


}*/

//show acitivty on led indictor, if any
void DecoderWS2812::set_activity(bool is_activity)
{
  if(_pwm_activity_pin<0) return; //invalid input, nothin to set
  uint slice_num = pwm_gpio_to_slice_num(_pwm_activity_pin);
  uint channel = pwm_gpio_to_channel(_pwm_activity_pin);
  pwm_set_chan_level(slice_num, channel, is_activity?DECODER_ACTIVITY_BRIGHTNESS:0);
}

// ---- generic IR decoder ----

DecoderGeneric::DecoderGeneric(uint8_t rxd_pin_index,int8_t pwm_activity_pin)
  : _rxd_pin_index(rxd_pin_index), _pwm_activity_pin(pwm_activity_pin){}

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
  set_activity(_buffer_ptr->is_activity(_rxd_pin_index,20'000));
  
}

bool DecoderGeneric::get_message(uint32_t *message, uint16_t &message_length)
{
  bool is_activity=_buffer_ptr->is_activity(_rxd_pin_index,20'000);//observe a 25ms timeout between end of message and start of next message on Sony TV report control
  if(is_activity || _buffer_index==0) return false;//nothing (ready) to report



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
  Serial.printf("IR RxD Activity: %d, %d, %d\n",_buffer_ptr->is_activity(_rxd_pin_index,20'000),gpio_get_function(_pwm_activity_pin),_pwm_activity_pin);
  uint32_t message[257];
  uint16_t message_len;
  bool is_message=get_message(message,message_len);
  if(is_message)
  {
    Serial.print("IR Message: ");
    for(uint16_t iter=0;iter<message_len;iter++) Serial.printf("%d, ",message[iter]);
    Serial.println();
  }else{
    Serial.print("IR Message: No Message\n");
  }
  //while(1) tight_loop_contents();//halt on first message received
}