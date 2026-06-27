//standup dma from logic analyzer into ring buffer
//read ring buffer
//extract state from each button
//apply threshold

//api: at 60 hz, report the "analog" (RC delay constant) reading for each button.  core0 can figure out how to thresehold/schmitt_trigger that
//actually, expose as an accessor method for one button, and update method (60 hz poll)

#include "touch.h"
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"
#include "hardware/regs/sysinfo.h"
#include "hardware/regs/addressmap.h"
#include "logic_analyzer.pio.h"
#include <stdio.h>
#include "pico/stdlib.h"

// -- class methods --

Touch::Touch(PIO pio){
  _pio=pio;
  //_sm_offset=sm_offset; //_sm_offset = pio_add_program(_pio, &logic_analyzer_program);
}

void Touch::begin(){
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

void Touch::end() {
    // 1. CRITICAL STEP: Abort the high-speed DMA ring buffer transfer first
    // This instantly cuts off active background writes to your _capture_buffer
    if (_dma_chan >= 0) {
        dma_channel_abort(_dma_chan);
        dma_channel_unclaim(_dma_chan);
        _dma_chan = -1; // Reset to uninitialized state
    }

    // 2. Turn off and release the PIO logic analyzer listener state machine
    if (_sm != (uint)-1) {
        pio_sm_set_enabled(_pio, _sm, false);
        pio_sm_clear_fifos(_pio, _sm); // Purge any remaining burst samples
        pio_sm_unclaim(_pio, _sm);
        _sm = (uint)-1;
    }

    // 3. Stop the hardware PWM charging clock slice
    uint slice_num = pwm_gpio_to_slice_num(FIRST_PIN_CAPTOUCH);
    pwm_set_enabled(slice_num, false);

    // 4. Return all touch and PWM stimulus pins back to a safe, quiet state
    for (uint8_t pin = FIRST_PIN_CAPTOUCH; pin < (FIRST_PIN_CAPTOUCH + CAPACITIVE_TOUCH_COUNT); pin++) {
        gpio_init(pin);
        gpio_set_dir(pin, GPIO_IN); // High-impedance input mode
        gpio_disable_pulls(pin);    // Ensure internal resistors don't leak current
    }
}

void Touch::update(uint32_t frame_id)
{
  //starting at capture_buffer_index, step forward until a transition on the PWM ((capture_buffer[index] ^ capture_buffer[index+1])&0x00000001) is found, where index wraps around CAPACITIVE_TOUCH_RING_BUFFER_SIZE
  //start working from (index+1) as the beginning of a PWM toggle
  //zero out _rc_decay[is_ping_ping][index]
  //step through each entry until &0x00000001 toggles.  At each step, keep a running sum of the upper 21 bits (time).  when one of these bits toggles (pin state toggle), add the (time) sum to that to _rc_decay[is_ping_ping][index];
  //divide all entries in _rc_decay[is_ping_ping][index] by the number of &0x00000001 toggles observed (convert total time waiting for pin to tottle into an average time for pin to toggle)
  for(uint8_t iter=0;iter<sizeof(_rc_decay[0])/sizeof(_rc_decay[0][0]);iter++) _rc_decay[_is_ping_pong][iter]=0;
  uint32_t latest_capture_buffer_index=(dma_hw->ch[_dma_chan].write_addr - (uint32_t)_capture_buffer)/sizeof(_capture_buffer[0]); //where data is currently being written to, don't try to access beyond this point (ring buffer) - fetch once at start to avoid inifintie while loop
  uint32_t pwm_toggle_count=0;//skip over the first few indexes until the first pwm toggle is found
  uint32_t running_pwm_time=0;//how far into a pwm pulse this is (number of cycles of a 25 Mhz clock)
  uint32_t rc_decay_prep[CAPACITIVE_TOUCH_COUNT]={};//only flush the readings at the end of a collect on 0->1 pwm transition to avoid shearing of a fractional update at the end of a collect
  while(1)
  {
    _capture_buffer_index=_capture_buffer_index%(sizeof(_capture_buffer)/sizeof(_capture_buffer[0]));//state previous
    uint32_t next_capture_buffer_index=(_capture_buffer_index+1)%(sizeof(_capture_buffer)/sizeof(_capture_buffer[0]));//state current (leas significnat 11 bits) and time between last state and current state (most signficiant 21 bits)
    if(latest_capture_buffer_index==_capture_buffer_index || latest_capture_buffer_index==next_capture_buffer_index) break;//stop when trying to look at area of memory that is currently being written into
    uint32_t xor_pin_state=_capture_buffer[_capture_buffer_index]^_capture_buffer[next_capture_buffer_index];//inspect the pins that changed state

    /*uint32_t pin_mask = _capture_buffer[next_capture_buffer_index] & 0x000007FF;
    String bin_str = "";
    for (int i = 10; i >= 0; i--) {
      bin_str += ((pin_mask >> i) & 1) ? "1" : "0";
    }
    Serial.printf("curr: %08X, next: %08X, idx: %6d, pins: %s, delay: %d\n",_capture_buffer[_capture_buffer_index],_capture_buffer[next_capture_buffer_index],_capture_buffer_index,bin_str.c_str(),_capture_buffer[next_capture_buffer_index]>>11);*/

    bool is_pwm_toggle=xor_pin_state&0x0000'0001;
    if(is_pwm_toggle && (_capture_buffer[next_capture_buffer_index]&0x0000'0001 || pwm_toggle_count>0))// && a 0->1 pwm transition as the starting point
    {//start of a pwm toggle
      //pwm_toggle_count++;
      running_pwm_time=0;
      if(_capture_buffer[next_capture_buffer_index]&0x0000'0001)//if 0->1 transition
      {
        pwm_toggle_count+=2;//at the 0->1 transition, there have been two toggles
        for(uint8_t iter=0;iter<CAPACITIVE_TOUCH_COUNT;iter++)
        {
          _rc_decay[_is_ping_pong][iter]+=rc_decay_prep[iter];
          rc_decay_prep[iter]=0;
        }
      }
    }else if(pwm_toggle_count>0){//within a pwm pulse
      running_pwm_time+=(1<<(32-CAPACITIVE_TOUCH_COUNT)) - (_capture_buffer[next_capture_buffer_index]>>CAPACITIVE_TOUCH_COUNT); //don't forget, the PIO is a cout-down timer, so need to subtract the reading from max value of 1<<21
      for(uint8_t iter=0;iter<CAPACITIVE_TOUCH_COUNT;iter++)
      {
        bool is_toggle=xor_pin_state&0x0000'0001;
        if(is_toggle) rc_decay_prep[iter]+=running_pwm_time;//precon: a single-clean toggle occurs within each pwm pulse - assume schmitt-trigger cleans off any de-bouncing issues at the 1+ kHz level
        xor_pin_state=xor_pin_state>>1;//look at the next pin
      }
    }
    _capture_buffer_index++;
  }
  //this is a little dirty where the logic doesn't check if an entire pwm pulse has completed, so may end up taking the sum across 65 or 66 pulses, dpeending on how long it takes for the pin to settle, before dividing by 66.  so this assumes edge effects are negligable (typically looking for a factor of >2x kind of difference, so presumably a few percent jitter is insignficiant)
  for(uint8_t iter=0;iter<CAPACITIVE_TOUCH_COUNT;iter++) _rc_decay[_is_ping_pong][iter]/=pwm_toggle_count-2;//take an average, precon: non-zero number of pwm toggles transpaired.  -2 to account for varaible being init'd at 2 on first use
  _is_ping_pong^=1;
  
  //now that _rc_decay has been updated with the intantaneous value, also update the _dc_offset accordingly
  for(uint8_t iter=0;iter<CAPACITIVE_TOUCH_COUNT;iter++)
  {//update max and min values...
    uint32_t rc_decay=_rc_decay[!_is_ping_pong][iter]; //read (!_ping_pong) the latest decay, update the dc_offset with that new info
    uint32_t old_min=_dc_offset[_is_ping_pong_dc][0][iter];//using the write index as a state machine (is_ping_pong)
    uint32_t old_max=_dc_offset[_is_ping_pong_dc][1][iter];
    uint32_t new_min=old_min==0?rc_decay:min(rc_decay,old_min);//if the running value started at zero, use the first sample as the base value
    uint32_t new_max=old_max==0?rc_decay:max(rc_decay,old_max);
    _dc_offset[_is_ping_pong_dc][0][iter]=new_min;
    _dc_offset[_is_ping_pong_dc][1][iter]=new_max;
  }
  if(frame_id%60==0)
  {//check if several conditions are met, and if so, update the dc_offset for all pins together (ping_pong update)
    bool is_valid_swap=true;
    for(uint8_t iter=1;iter<CAPACITIVE_TOUCH_COUNT;iter++) //0th index is the pwm strobe itself, so ignore that
    {
      is_valid_swap&=_dc_offset[_is_ping_pong_dc][0][iter]>=CAPACITIVE_TOUCH_MIN_IDLE_DECAY;
      is_valid_swap&=_dc_offset[_is_ping_pong_dc][1][iter]<=CAPACITIVE_TOUCH_MAX_IDLE_DECAY;
      is_valid_swap&=(_dc_offset[_is_ping_pong_dc][1][iter]-_dc_offset[_is_ping_pong_dc][0][iter])<=CAPACITIVE_TOUCH_MAX_IDLE_RANGE; //ensure max-min isn't too wide (indicates user activity somewhere on screen)
    }
    if(is_valid_swap) _is_ping_pong_dc^=1;//if all conditions met for an idle state, then update the min/max definition
    for(uint8_t iter=0;iter<CAPACITIVE_TOUCH_COUNT;iter++)
    {//clear the opposing pin_pong buffer to restart min/max comutation on next update()
      _dc_offset[_is_ping_pong_dc][0][iter]=0;
      _dc_offset[_is_ping_pong_dc][1][iter]=0;
    }
  }

  //update which button is considered down right now (single-touch support)
  uint32_t reading=0;
  uint8_t out_id=0;
  for(int iter=1;iter<CAPACITIVE_TOUCH_COUNT;iter++)
  {
    uint32_t this_reading=get_capacitive_touch(iter);
    if(this_reading>=sensitivity && (reading==0 || this_reading>reading) )
    {//if the capacitance is sufficient to equal a finger, and the reading is better than before, consider this the touched button
      reading=this_reading;//PRECON: assuming varaible dc-offset between buttons, but equal gain once a finger is present, so ignore gain correction here
      out_id=iter;
    }
  }
  _button_down=out_id;
}

//make a hard determiantion of which button is down and assume the rest ofare not
uint8_t Touch::get_down_button(){ return _button_down; }

//index 0 is pwm pin, cap touch are indexes 1-10
//gets "analog" reading of each pin, dc-bias-corrected
uint32_t Touch::get_capacitive_touch(uint8_t index)
{
  if(index>CAPACITIVE_TOUCH_COUNT) return 0;
  return max(_rc_decay[!_is_ping_pong][index],_dc_offset[!_is_ping_pong_dc][0][index])-_dc_offset[!_is_ping_pong_dc][0][index];
}

void Touch::debug()
{
  Serial.print("Touch: ");
  for(int iter=1;iter<11;iter++) Serial.printf("%d=%4d, ",iter,get_capacitive_touch(iter));
  Serial.printf("down_id: %d\n",get_down_button());
}