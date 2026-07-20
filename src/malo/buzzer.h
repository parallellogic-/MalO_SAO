#pragma once

#include "dma_control_block.h"
#include "pulse_chain.h"

//#define BUZZER_FREQUENCY_HZ 2'000.0f //lowest frequency the system generates (256 counts on period)
#define BUZZER_PIN 25

class Buzzer {
private:
  PulseChain pulse_chain=PulseChain();
  bool _is_master_disable=false;
  uint8_t _pwm_pin=0;
  uint64_t _last_update_us=0;//when the note started to play
  uint64_t _duration_us=0;//how long to play note for
public:
  Buzzer(){}
  void begin(PIOProgramManager &pio_program_manager,uint8_t pwm_pin=BUZZER_PIN){
    //float _base_frequency_hz=BUZZER_FREQUENCY_HZ;
    _pwm_pin=pwm_pin;
//    pulse_chain.begin(pio_program_manager,pwm_pin,_base_frequency_hz); //system creashes after file system ls() call (out of ram?  heap/stack intersection?)

    clear();
  }
  void update(){ if((time_us_64()-_last_update_us)>(_duration_us) || _is_master_disable) clear(); }
  void end(){}//TODO

  /*bool is_busy(){ return pulse_chain.is_busy(); }
  bool append_note(uint8_t period,uint8_t duty,uint16_t cycle_count){ return pulse_chain.append_note(period,duty,cycle_count); }//assert duty<=period.  frequency produced is BUZZER_FREQUENCY_HZ/period
  bool append_tone(float frequency_hz, float duration_ms, bool is_enabled) {
      // 1. Handle the mute/disabled safety state up front
      if (!is_enabled || frequency_hz <= 0.0f) {
          // To play silence, set period to max (255) and duty to 0
          // Use the baseline BUZZER_FREQUENCY_HZ to figure out how many cycles fit in duration_ms
          uint16_t silence_cycles = (uint16_t)((duration_ms / 1000.0f) * (BUZZER_FREQUENCY_HZ / 255.0f));
          return append_note(255, 0, silence_cycles);
      }

      // 2. Calculate correct hardware timer period (Inversely proportional)
      float raw_period = BUZZER_FREQUENCY_HZ / frequency_hz;
      
      // Clamp to uint8_t bounds (1 to 255) to prevent hardware divide-by-zero or overflow
      if (raw_period > 255.0f) raw_period = 255.0f;
      if (raw_period < 1.0f)  raw_period = 1.0f;
      uint8_t period = (uint8_t)raw_period;

      // 3. Calculate 50% duty cycle relative to the period
      uint8_t duty = period / 2;

      // 4. Calculate total waveform cycles required to fulfill duration_ms
      uint16_t cycle_count = (uint16_t)((duration_ms / 1000.0f) * frequency_hz);

      return append_note(period, duty, cycle_count); 
  }
  bool play() { if(_is_master_disable) return pulse_chain.clear(); return pulse_chain.play(); }
  */
  void play_tone(float frequency_hz,float duration_ms,bool is_enabled){
    if(_is_master_disable) is_enabled=false;

    //work-around is to skip DMA buffer and command manually:
    gpio_set_function(_pwm_pin, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(_pwm_pin);
    uint channel = pwm_gpio_to_channel(_pwm_pin);
    pwm_set_wrap(slice_num, 255);
    pwm_set_chan_level(slice_num, channel, is_enabled?127:0);
    float sys_clk_hz = (float)clock_get_hz(clk_sys);
    float dynamic_div = sys_clk_hz / (frequency_hz * 256.0f);
    pwm_set_clkdiv(slice_num, dynamic_div);
    pwm_set_enabled(slice_num, true); 
    _last_update_us=time_us_64();
    _duration_us=1000*duration_ms;
  }
  void play_tone(float frequency_hz,float duration_ms){ play_tone(frequency_hz,duration_ms,true); }
  void append_tone(float frequency_hz,float duration_ms,bool is_enabled){}
  void clear(){ play_tone(4000,1,false); }
  void play(){}

  void set_master_disable(bool is_master_disable){_is_master_disable=is_master_disable; }
};