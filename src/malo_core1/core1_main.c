#include "pico/stdlib.h"
#include "pico/multicore.h"

#define LED_CORE1_PIN 38

// This function runs exclusively on Core 1
void malo_core1_entry(void) {
//void __no_inline_not_in_flash_func(malo_core1_entry)(void) {
    // Initialize GPIO 5
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

