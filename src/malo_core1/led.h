#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <hardware/pio.h>
#include <hardware/dma.h>
#include "charlieplex.pio.h"
#include "hardware/regs/pads_bank0.h"
#include "hardware/structs/padsbank0.h"

#define CHARLIEPLEX_LED_COUNT (24*2) // 24 red-green LEDs = 48 elements

// 1. Safe Array Declaration (Memory allocated in led.cpp)
extern uint16_t const CHARLIEPLEX_PINOUT_CONFIG[2][CHARLIEPLEX_LED_COUNT];

// 2. Hide the C++ Class definition from pure C modules
#ifdef __cplusplus
class Charlieplex {
  private:
    uint8_t _api_brightness[CHARLIEPLEX_LED_COUNT];
    uint32_t _charlieplex_list[2][CHARLIEPLEX_LED_COUNT+1];
    uint8_t _max_effective_led_count = CHARLIEPLEX_LED_COUNT;
    uint8_t _charlieplex_index = 0;
    uint32_t* _current_list_ptr; 
    uint8_t _pio_index = 0;
    uint8_t _first_pin;
    PIO _pio = pio0;
    static uint _sm_offset;
    uint _sm;
    int _data_chan;
    int _ctrl_chan;
  public:
    Charlieplex(bool is_upper);
    void begin();
    bool flush();
    bool set_brightness(uint8_t index, uint8_t brightness);
    bool set_effective_led_count(uint8_t count);
};
#endif

