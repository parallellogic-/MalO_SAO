/*#include "ir_rxd.h"

// ---- SharedDecoderBuffer ----

SharedDecoderBuffer::SharedDecoderBuffer(PIO pio,uint8_t rxd_first_pin,uint8_t rxd_pin_count)
  : _pio(pio), _rxd_first_pin(rxd_first_pin)), _rxd_pin_count(rxd_pin_count){}

SharedDecoderBuffer::begin(){
  pio_set_gpio_base(_pio, 16);//need to use >pin 32 for this pio

  if(_sm_offset<0) _sm_offset=pio_add_program(pio1, &logic_analyzer_program);

  _sm=pio_claim_unused_sm(_pio, true); 
  for(uint8_t pin=FIRST_PIN_CAPTOUCH;pin<(FIRST_PIN_CAPTOUCH+CAPACITIVE_TOUCH_COUNT);pin++)
  {//init the pins
    gpio_disable_pulls(pin);
    pio_gpio_init(_pio, pin);
    gpio_set_input_enabled(pin, true);
    gpio_disable_pulls(pin);
  }

  // 1. Configure the Pin Mux for PWM Output
  gpio_set_function(FIRST_PIN_CAPTOUCH, GPIO_FUNC_PWM);
  
  // 2. Force the input buffer ON so PIO can "see" the pin state
  gpio_set_input_enabled(FIRST_PIN_CAPTOUCH, true);
  gpio_disable_pulls(FIRST_PIN_CAPTOUCH);

  // 3. Configure the PWM Peripheral
  uint slice_num = pwm_gpio_to_slice_num(FIRST_PIN_CAPTOUCH);
  uint channel = pwm_gpio_to_channel(FIRST_PIN_CAPTOUCH);
  
  // Set the clock divider to scale 150MHz down to 4kHz with a 255 wrap.  /4.0f makes this 16kHz
  pwm_set_clkdiv(slice_num, 146.484f/4.0f);

  pwm_set_wrap(slice_num, 256-1);                 // Set frequency period
  pwm_set_chan_level(slice_num, channel, 256/2);  // 50% Duty cycle
  pwm_set_enabled(slice_num, true);             // Start generating PWM

  // 4. Configure the PIO State Machine to listen
  pio_sm_config c = logic_analyzer_program_get_default_config(_sm_offset);
  
  // Set the IN pins to start at our PWM pin
  sm_config_set_in_pins(&c, FIRST_PIN_CAPTOUCH);
  sm_config_set_in_pin_count(&c, CAPACITIVE_TOUCH_COUNT);

  // RP2350 requires the FIFO to be joined to handle high-speed bursts
  sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_RX);

  // Initialize and start the state machine
  pio_sm_init(_pio, _sm, _sm_offset, &c);
  pio_sm_set_enabled(_pio, _sm, true);

  // 4. DMA Setup
  _dma_chan = dma_claim_unused_channel(true);
  dma_channel_config dma_c = dma_channel_get_default_config(_dma_chan);
  channel_config_set_transfer_data_size(&dma_c, DMA_SIZE_32);
  channel_config_set_read_increment(&dma_c, false);
  channel_config_set_write_increment(&dma_c, true);
  channel_config_set_ring(&dma_c, true, __builtin_ctz(sizeof(_capture_buffer))); // 10+2 bits = 1024*4 words --> +2 fudge factor needed (for uint8 to uint32 adaption?)) //10+2+2
  channel_config_set_dreq(&dma_c, pio_get_dreq(_pio, _sm, false));

  dma_channel_configure(_dma_chan, &dma_c, _capture_buffer, &_pio->rxf[_sm], 0xFFFFFFFF, true);
}
SharedDecoderBuffer::end(){
  
}

uint16_t SharedDecoderBuffer::get_buffer_index()
{

}

bool SharedDecoderBuffer::is_activity(uint8_t pin_index,uint32_t timeout_us)
{
  uint16_t current_index=get_buffer_index();//find out where we currently are
  uint16_t stop_index=current_index;
  bool is_init=false;
  bool static_state=0;//if this state is held throughout the ring buffer, then there is no activity (looking for transitions away from this state)
  uint32_t running_cycle_count=0;
  uint32_t done_cycle_count=timeout_us*25;//logic analyzer runs at 25 MHz = 0.04us per tick
  do{//decrement first, but beware ring buffer wrap-around
    if(current_index==0) current_index=sizeof(_data_buffer)/sizeof(_data_buffer[0]);
    else current_index--;
    if(!is_init)
    {
      static_state=(_data_buffer>>pin_index)&0x01;
      is_init=true;
    }
  }
  while(current_index!=stop_index)
  {
    bool current_state=(_data_buffer[current_index]>>pin_index)&0x01;
    if(current_state^static_state) return true; //found a transition
    running_cycle_count+=current_state>>LOGIC_ANALYZER_PIN_COUNT;//logic analyzer is bit-packing per this pin count, even if these many aren't being monitored by SharedDecoderBuffer
    if(running_cycle_count>done_cycle_count) return false;//if went back in time father than desired without finding a transition, then report there was no transitions
  }
  return false;//if looped through entire ring buffer without finding a transition, report no transisiotn
}


// ---- DecoderWS2812 ----

DecoderWS2812::DecoderWS2812(SharedDecoderBuffer* buffer,uint8_t rxd_pin_index,int8_t pwm_activity_pin=-1)
  : _buffer_ptr(buffer), _rxd_pin_index(rxd_pin_index), _pwm_activity_pin(pwm_activity_pin){}

DecoderWS2812::begin(){
  if(pwm_activity_pin>=0 && (gpio_get_dir(_pwm_activity_pin) == GPIO_IN))
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

DecoderWS2812::update(){
  set_activity(_buffer_ptr->is_activity(_rxd_pin_index,200'000));
  if(_period==0) _period=get_period(); //if period unset, then determine period
  if(_period==0) return;//if still unset, then no action to perform
  //update state machine...

}

uint32_t DecoderWS2812::get_message_start()
{
  //look through backward from the current pointer at the most recent 8x 0->1 transitions. store the median of that as the period (but don't push live until start of message found,otherwise just looking at mid-message jibberish)
  //then keep going back until a gap of >=40*period is found(or wrap around back to current pointer)

  uint16_t latest_buffer_index=_buffer_ptr->get_buffer_index();


}

//show acitivty on led indictor, if any
void DecoderWS2812::set_activity(bool is_activity)
{
  if(_pwm_activity_pin<0) return; //invalid input, nothin to set
  uint slice_num = pwm_gpio_to_slice_num(_pwm_activity_pin);
  uint channel = pwm_gpio_to_channel(_pwm_activity_pin);
  pwm_set_chan_level(slice_num, channel, is_activity?DECODER_ACTIVITY_BRIGHTNESS:0);
}*/