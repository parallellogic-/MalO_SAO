#pragma once

//precon for PIO: mic has L/R pulled high
#define PDM_DATA_PIN 14
#define PDM_CLK_PIN  PDM_DATA_PIN+1 //precon pinout for PIO

#define MICROPHONE_SAMPLE_BUFFER 512 //150 Mhz / 63 down-sample ~=2.4 MHz.  2.4 Mhz at 256 PIO pdm2pcm =9.3 kHz.  9.3 kHz at 16 ms is 155 samples.  Double the cyclical buffer size up to next power of 2: 512

class Microphone{
  private:
    PIO _pio;
    uint _sm;
    uint8_t _data_pin=0;
    uint8_t _clock_pin=0;
    int _data_dma;
    uint8_t _cyclical_buffer[MICROPHONE_SAMPLE_BUFFER] __attribute__((aligned(MICROPHONE_SAMPLE_BUFFER)))={};
    uint32_t _last_dma_update_address=0;//address where the last dma update left off
    float _mean_square=0;
  public:
    Microphone(PIO pio=pio0,uint8_t data_pin=PDM_DATA_PIN,uint8_t clock_pin=PDM_CLK_PIN);
    void begin();
    void update();
    void end();
    float get_mean_square();
};