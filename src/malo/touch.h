#pragma once

#define FIRST_PIN_CAPTOUCH 26 //PWM pin
#define CAPACITIVE_TOUCH_COUNT 11//first PWM pin, then 10 cap touch buttons
#define CAPACITIVE_TOUCH_RING_BUFFER_SIZE 4096 //store run-length-encoded transitions

class Touch{
  private:
    uint32_t _capture_buffer[CAPACITIVE_TOUCH_RING_BUFFER_SIZE] __attribute__((aligned(CAPACITIVE_TOUCH_RING_BUFFER_SIZE*sizeof(uint32_t))));
    uint16_t _capture_buffer_index=0;//index of the end of the last update
    uint32_t _rc_decay[2][CAPACITIVE_TOUCH_COUNT]; //steps of a 25 Mhz clock until pin toggles.
    bool _is_ping_pong=false;//write to [is_ping_pong], read from [!is_ping_pong]
    PIO _pio = pio0;
    uint _sm;
    uint _sm_offset;//only upload the PIO program once
    int _dma_chan;//dma for moving out data from PIO into Flash
  public:
    Touch(PIO pio);
    void begin(uint sm_offset); //claim PIO, start DMA
    void end(); //claim PIO, start DMA
    void update(); //60 Hz state update, toggles ping_pong
    uint32_t get_capacitive_touch(uint8_t index);//low values means button is unpressed, high value means button is pressed
};