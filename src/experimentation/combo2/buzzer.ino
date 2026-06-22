#include "buzzer.h"

Buzzer::Buzzer(int32_t timeout_ms) : _timeout_ms(timeout_ms) {
}

bool Buzzer::update(){
  if(_is_off) return true;//save a bit of compute time by skipping over then buzzer is off
  if( ((time_us_64()-_last_update_us)/1000>_timeout_ms) && (_timeout_ms>=0) ) set_off();//zero duty cycles is no tone
  return true;
}

void Buzzer::begin(uint8_t gpio_pin){
    uint slice_num = pwm_gpio_to_slice_num(gpio_pin);
    uint channel = pwm_gpio_to_channel(gpio_pin);
    gpio_set_function(gpio_pin, GPIO_FUNC_PWM);

    set_off();
}

void Buzzer::end(uint8_t gpio_pin) {
    // 1. Force the tone generation completely off
    set_off();
    
    // 2. Disable the physical hardware PWM slice timer
    uint slice_num = pwm_gpio_to_slice_num(gpio_pin);
    pwm_set_enabled(slice_num, false);
    
    // 3. Disconnect the pin from the PWM engine and reset it to a standard input
    // This removes the pin from the high-speed peripheral bus grid
    gpio_init(gpio_pin);
    gpio_set_dir(gpio_pin, GPIO_IN);
}

void Buzzer::set_off(){
  set_tone(1000.0,0);
}

void Buzzer::set_tone(float frequency_hz, float duty_cycle_percent){
  _is_off=duty_cycle_percent==0.0;

/*  float sys_clk = (float)clock_get_hz(clk_sys);
  uint32_t top = 20000; 

  // Formula: sys_clk / (freq * top) = clock_divider
  float clk_div = sys_clk / (frequency_hz * (float)top);

  // RP2350 hardware clock divider register supports values from 1.0 up to 255.9375
  // If the calculated divider is too large (very low frequency), clamp it and adjust TOP
  if (clk_div > 255.0f) {
      clk_div = 255.0f;
      top = (uint32_t)(sys_clk / (tone_hz * clk_div));
  }

  // 5. Apply the calculated timing values to the hardware registers
  pwm_set_clkdiv(slice_num, clk_div);
  pwm_set_wrap(slice_num, top - 1); // Wrap register counts from 0 to (TOP - 1)
*/
}

