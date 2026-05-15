#ifndef SSD1327_H
#define SSD1327_H

#include <Arduino.h>
#include "hardware/spi.h"
#include "hardware/dma.h"

// Data buffer for transmission
// Use a large enough buffer for your needs
#define SSD1327_BUFFER_SIZE 128*128/2 //one nibble per pixel (0-15 grayscale value per pixel), 128*128 pixels, 1.5"

class SSD1327 {
  private:
    spi_inst_t* _spi_port;
    uint32_t _spi_baud;
    uint8_t _cs;
    uint8_t _dc;
    uint8_t _mosi;
    uint8_t _sclk;
    uint8_t _tx_buffer[2][SSD1327_BUFFER_SIZE];//one buffer is being written to, one buffer is pushed to hardware
    bool _display_index=0;//which index is being written to the hardware
    int _dma_tx_channel;
  public:
    SSD1327();
    SSD1327(uint8_t cs, uint8_t dc, uint8_t mosi, uint8_t sclk,spi_inst_t* spi_port,uint32_t spi_baud); 
    void begin();
    void draw();
    void flush();
    uint8_t* get_buffer();
    uint8_t* get_buffer(bool is_black);
    void send_data_dma(const uint8_t *data, size_t len, bool dc_value);
};

#endif

#ifndef LED_MATRIX_H
#define LED_MATRIX_H

#include <hardware/pio.h>
#include <hardware/dma.h>
#include "charlie.pio.h"
#include "hardware/regs/pads_bank0.h"
#include "hardware/structs/padsbank0.h"

uint8_t const CHARLIPLEX_LED_COUNT=24*2;//24 reg-green LEDs
uint8_t const CHARLIPLEX_FRAME_BUFFER_DEPTH=2;//one being read from by PIO, one being buffered into - then ping-pong swap after flush()

uint16_t const CHARLIEPLEX_PINOUT_CONFIG[2][CHARLIPLEX_LED_COUNT]={//index 0: lower LEDs config (under screen), index 1: upper LEDs (in hair)
{//lower LEDs
//red [0..23] left-to-right from led_matrix.ods.  MSB is dir (output=1, float=0), LSB is pin (1=high, 0=low)
0x4060,0x4048,0x020A,0x0222,0x0282,0x0242,0x0109,0x0121,0x0141,0x0111,0x0103,0x0181,0x2060,0x0848,0x080A,0x2022,0x4042,0x8082,0x8081,0x4041,0x2021,0x0809,0x1011,0x0203,

//green 24..47 left-to-right
0x80A0,0x8088,0x1018,0x1030,0x1090,0x1050,0x040C,0x0424,0x0444,0x0414,0x0406,0x0484,0x20A0,0x0888,0x0818,0x2030,0x4050,0x8090,0x8084,0x4044,0x2024,0x080C,0x1014,0x0206
},{//upper LEDs
//red CW around her face, then mostly left-to-right (and bottom-to-top along diagonals)
0x0103,0x0111,0x8082,0x8090,0x8081,0x80A0,0x0406,0x0414,0x0405,0x0424,0x0444,0x0484,0x0203,0x1011,0x0282,0x1090,0x0181,0x20A0,0x0206,0x1014,0x0105,0x2024,0x4044,0x8084,

//green
0x2022,0x2030,0x4042,0x4050,0x4041,0x4060,0x080A,0x0818,0x0809,0x0828,0x0848,0x0888,0x0222,0x1030,0x0242,0x1050,0x0141,0x2060,0x020A,0x1018,0x0109,0x2028,0x4048,0x8088
}};

class Charlieplex{
  private:
    uint8_t _api_brightness[CHARLIPLEX_LED_COUNT];//write brightness values here for the frame currently being developed (red in lower 24 indexes, green in the upper 24)
    uint32_t _charliplex_list[CHARLIPLEX_FRAME_BUFFER_DEPTH][CHARLIPLEX_LED_COUNT+1];//31..16 is brightness, 15..8 is input(0) vs output(1) for 8 pins, 7..0 is high(1) vs low(0) for 8 pins.  +1 for a sleep statement for brightness stabalization
    uint8_t _max_effective_led_count=CHARLIPLEX_LED_COUNT;//max number of LED elements that are expected to be on simultaneously: drives refresh frame rate
    uint8_t _charliplex_index = 0;//the buffer index being read from for the current DMA operation
    uint32_t* _current_list_ptr; // The "Next" list pointer
    uint8_t _pio_index=0;
    uint8_t _first_pin;
    PIO _pio = pio0;
    static uint _sm_offset;//only upload the PIO program once
    uint _sm;
    int _data_chan;//dma for moving out data
    int _ctrl_chan;//dma to loop the other dma (optional: could make a 64-element dma loop, with 0 wait states at end to save a DMA channel)
  public:
    Charlieplex(bool is_upper);
    void begin();
    void flush();
    void set_brightness(uint8_t index,uint8_t brightness);
    void set_max_effective_led_count(uint8_t count);
};

#endif

#ifndef TOUCH_H
#define TOUCH_H


#endif