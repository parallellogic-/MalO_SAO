#pragma once

#include "hardware/pio.h"
#include "pio_program_manager.h"

#define FIRST_PIN_CAPTOUCH 26 //PWM pin
#define CAPACITIVE_TOUCH_COUNT 11//first PWM pin, then 10 cap touch buttons
#define CAPACITIVE_TOUCH_RING_BUFFER_SIZE 4096 //store run-length-encoded transitions
#define CAPACITIVE_TOUCH_MIN_IDLE_DECAY 100 //how low the decay time is expected to be before it's considered invalid, roughly a few pF
#define CAPACITIVE_TOUCH_MAX_IDLE_DECAY 250 //in the 10-ish pF area
#define CAPACITIVE_TOUCH_MAX_IDLE_RANGE 5 //max-min when the screen is idle
#define CAPACITIVE_TOUCH_SENSITIVITY 32 //lower is more sensitive and prone to false alarms

class Touch{
  private:
    uint32_t _capture_buffer[CAPACITIVE_TOUCH_RING_BUFFER_SIZE] __attribute__((aligned(CAPACITIVE_TOUCH_RING_BUFFER_SIZE*sizeof(uint32_t))));
    uint16_t _capture_buffer_index=0;//index of the end of the last update
    uint32_t _rc_decay[2][CAPACITIVE_TOUCH_COUNT]; //count the number of steps of a 25 Mhz clock until pin toggles.
    bool _is_ping_pong=false;//write to [is_ping_pong], read from [!is_ping_pong]
    uint32_t _dc_offset[2][2][CAPACITIVE_TOUCH_COUNT]={{0,188,153,147,155,141,140,147,151,146,149},{0,188,153,147,155,141,140,147,151,146,149}};//[_is_ping_pong][is_max][button_id] look for the min/max reading over the past 2 seconds and store that here (if satisfies range checks)
    bool _is_ping_pong_dc=true;//cycles less frequently than the 
    //PIO _pio;
    //uint _sm;
    //inline static int _sm_offset=-1;//only upload the PIO program once
    int _dma_chan;//dma for moving out data from PIO into Flash
    uint8_t _sensitivity=CAPACITIVE_TOUCH_SENSITIVITY;//lower is more sensitive and prone to false alarms - beware setting this in gui may make buttons too hard to press to revert setting...
    volatile uint8_t _button_down=0;//which button is currently pressed down, update once in update() so other callers can have quick access
  public:
    Touch();//PIO pio=pio1);
    void begin(PIOProgramManager &pio_program_manager); //claim PIO, start DMA
    void end(); //claim PIO, start DMA
    void update(uint32_t frame_id); //60 Hz state update, toggles ping_pong
    void debug();
    uint8_t get_down_button();//0 for no touch, 1-10 for which button is touched
    uint32_t get_capacitive_touch(uint8_t index);//low values means button is unpressed, high value means button is pressed
};