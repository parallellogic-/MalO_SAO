#pragma once

#include <stdint.h>
#include <stdbool.h>
//#include "led.h"

//Charlieplex led_lower(0);
//Charlieplex led_upper(1);

#ifdef __cplusplus
extern "C" {
#endif

// This function runs exclusively on Core 1 and is safe for both C and C++ files
void malo_core1_entry(void);

#ifdef __cplusplus
}
#endif

//bool set_charlieplex_led(uint8_t bank_index,uint8_t led_index,uint8_t brightness);
//bool set_effective_led_count(uint8_t bank_index,uint8_t led_count);
//void flush(uint8_t bank_index);
