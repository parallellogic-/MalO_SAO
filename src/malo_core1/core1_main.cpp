#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "core1_main.h"

#define LED_CORE1_PIN 38

void core1_begin(){
  //Serial.println("Init LEDs...");
  //led_upper.begin();
  //led_lower.begin();
  
  
}

// This function runs exclusively on Core 1
void malo_core1_entry(void) {
//void __no_inline_not_in_flash_func(malo_core1_entry)(void) {
    // Initialize GPIO 5
    core1_begin();
    
    gpio_init(LED_CORE1_PIN);
    gpio_set_dir(LED_CORE1_PIN, GPIO_OUT);

    // Infinite blink loop for Core 1
    while (true) {
        /*gpio_put(LED_CORE1_PIN, 1);
        mp_hal_delay_ms(500);//sleep_ms(500);
        gpio_put(LED_CORE1_PIN, 0);
        mp_hal_delay_ms(500);*///sleep_ms(500);
        gpio_put(LED_CORE1_PIN, (to_ms_since_boot(get_absolute_time()) % 1000) < 500);
    }
}

/*bool set_charlieplex_led(uint8_t bank_index,uint8_t led_index,uint8_t brightness)
{
  if(bank_index>=2) return false;
  if(led_upper==1) return led_upper.set_charlieplex_led(led_index,brightness);
  return led_lower.set_charlieplex_led(led_index,brightness);
}

bool set_effective_led_count(uint8_t bank_index,uint8_t led_count)
{
  if(bank_index>=2) return false;
  if(led_upper==1) return led_upper.set_effective_led_count(led_count);
  return led_lower.set_effective_led_count(led_count);
}

void flush(uint8_t bank_index)
{
  if(bank_index>=2) return false;
  if(led_upper==1) led_upper.flush();
  else led_lower.flush();
}*/
