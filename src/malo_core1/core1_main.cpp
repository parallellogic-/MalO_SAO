#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "core1_main.h"

#define LED_CORE1_PIN 38 //debug green

Charlieplex led_lower(false);
Charlieplex led_upper(true);

volatile uint32_t is_core1_halt = 0;
volatile uint32_t frame_id = 0;

void core1_begin(){
  is_core1_halt = 0;
  frame_id = 0;

  //Serial.println("Init LEDs...");
  led_upper.begin();
  led_lower.begin();
  
  
}

// This function runs exclusively on Core 1
void malo_core1_entry(void) {
    core1_begin();
    
    gpio_init(LED_CORE1_PIN);
    gpio_set_dir(LED_CORE1_PIN, GPIO_OUT);

    //uint32_t previous_frame_id=0;
    // Infinite blink loop for Core 1
    while (!is_core1_halt) {
        gpio_put(LED_CORE1_PIN, (to_ms_since_boot(get_absolute_time()) % 1000) < 500);
    }
}

extern "C" {

bool set_charlieplex_led(uint8_t bank_index,uint8_t led_index,uint8_t brightness)
{
  if(bank_index>=2) return false;
  if(bank_index==1) return led_upper.set_brightness(led_index,brightness);
  return led_lower.set_brightness(led_index,brightness);
}

bool set_effective_led_count(uint8_t bank_index,uint8_t led_count)
{
  if(bank_index>=2) return false;
  if(bank_index==1) return led_upper.set_effective_led_count(led_count);
  return led_lower.set_effective_led_count(led_count);
}

bool flush(uint8_t bank_index)
{
  if(bank_index>=2) return false;
  if(bank_index==1) return led_upper.flush();
  return led_lower.flush();
}

} // extern "C"
