#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
#include "led.h"

// C++ objects must stay inside the __cplusplus guard
extern Charlieplex led_lower;
extern Charlieplex led_upper;

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
