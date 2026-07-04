#pragma once

#include "hardware/pio.h"
#include "logic_analyzer.pio.h"

//shared class that handles both the ir_rxd, as well as ws2812 decode on gpio 1/2
//choose ws2812 protocol for IR Tx/rx so simplify code base (but with different time scales - 38/4 khz minimum lenght for a '1' on ir rx)
//0.08 mW/m2 is lower threshold? 

//IR issue expected to be shortening of '1' signal by ~25%, so target 42% and 88% duty cycle (?)

#define SHARED_BUFFER_LENGTH (256)//2 transitions, 8 bits = 16 bytes, assuming no actiivty on other channels.
//800 kHz * 2 1/0 transitions * 16.6 ms = 26560 --> 32768 ring buffer size (max size) if doing real WS2812 decode.  but that's for continuous stream of WS2812.
//Max should be 24 LEDs, *2 banks, *3 colors, *8 bits, *2 1/0 transitions = 2304 --> 4096 (PRECON: user sends max 1 frame of data per frame)
#define IR_RXD_PIN 43 //need to assign pull-up on IR RxD pin
#define SHARED_BUFFER_FIRST_PIN 43
#define SHARED_BUFFER_PIN_COUNT 3 //IR_RXD, SAO_GP1, SAO_GP2
#define DECODER_MAX_MESSAGE_LENGTH 257 //max number of bytes, including error correction, that are expected to be received before a timeout (40 baud cycles) is expect to be received
#define DECODER_ACTIVITY_BRIGHTNESS (65535/10) //how long of 65535 should the PWM indicator be ON for each cycle (less is dimmer)

class SharedDecoderBuffer{
  private: 
    uint32_t _capture_buffer[SHARED_BUFFER_LENGTH] __attribute__((aligned(SHARED_BUFFER_LENGTH*sizeof(uint32_t))));
    int _dma_chan=-1;
    //PIO _pio=pio1;
    //int _sm=-1;
    uint8_t _rxd_first_pin;//first GPIO pin that the pio_sm will be tied to for the logic analyzer (equates to pin_index 0 in later queries)
    uint8_t _rxd_pin_count;//how many sequential pins to configure as connected to the logic analyzer (max is LOGIC_ANALYZER_PIN_COUNT)
  public:
    SharedDecoderBuffer(uint8_t rxd_first_pin=SHARED_BUFFER_FIRST_PIN,uint8_t rxd_pin_count=SHARED_BUFFER_PIN_COUNT);
    void begin(PIOProgramManager &pio_program_manager);//setup DMA ring
    //nothing to update, just is a constant ring-buffer loop
    void end(); //DMA and sm/pio dispose
    uint16_t get_buffer_index();//returns the index within the ring buffer where the latest reading was stored
    void get_sample(uint8_t pin_index,uint16_t sample_index,uint32_t &cycles,bool &value);//returns the number of 25 MHz cycles since the last state change (backward looking), and returns the new value the pin is now at after then transition (forward looking)
    bool is_activity(uint8_t pin_index,uint32_t timeout_us);//true if there has been any 1/0 or 0/1 transitions in the past timeout_us
};

class DecoderWS2812{
  private:
    //just doing static variables here vs standing up a separate class and sharing pointers to it among mulitple consumers...
    SharedDecoderBuffer *_buffer_ptr=nullptr;
    int16_t _buffer_index=0;//location of last read from ring-buffer (or first observed transition)
    uint8_t _decode_buffer[DECODER_MAX_MESSAGE_LENGTH];
    uint16_t _decode_index=0;//index where next byte should be written to
    uint32_t _period=0;//emperically derived period (look at first byte received and look at most common period of 0->1 transitions and use that to decode)
    uint8_t _rxd_pin_index; //index of the pin wihtin the pio_sm output to inspect (0 to LOGIC_ANALYZER_PIN_COUNT-1)
    int8_t _pwm_activity_pin=-1; //where to route is_activity indication to during update()

    void set_activity(bool is_activity);//flush activity state to PWM LED
    uint32_t get_period(); //helper to determine the spacing between bits
  public:
    DecoderWS2812(uint8_t rxd_pin_index,int8_t pwm_activity_pin=-1);
    void begin(SharedDecoderBuffer* buffer);
    void update(); //update decode state machine
    void debug();
    //void end();

    bool get_message(uint8_t &message, uint16_t &message_length, uint32_t &period); //if message is found, store into &message (expect max 256 character size)
};

//TODO: generic IR decoder class here to store the run-length encoded pulse lengths (structure akin to the IEEE float/double protocol: the first bit (duration) is a 1 and go from there) for lookup against industry standard protocol definitions
class DecoderGeneric{
  private:
    SharedDecoderBuffer *_buffer_ptr=nullptr;
    int16_t _buffer_index=0;//location of last read from ring-buffer (or first observed transition)
    uint32_t _decode_buffer[DECODER_MAX_MESSAGE_LENGTH];
    uint16_t _decode_index=0;//index where next byte should be written to
    uint8_t _rxd_pin_index; //index of the pin wihtin the pio_sm output to inspect (0 to LOGIC_ANALYZER_PIN_COUNT-1)
    int8_t _pwm_activity_pin=-1; //where to route is_activity indication to during update()
  public:
    DecoderGeneric(uint8_t rxd_pin_index,int8_t pwm_activity_pin=-1);
    void begin(SharedDecoderBuffer* buffer);
    void update();
    void debug();
    //void end();

    void set_activity(bool is_activity);//flush activity state to PWM LED
    bool get_message(uint32_t *message, uint16_t &message_length); //returns a series of '1'/'0' durations in us, starting with a '1' duration.  true on message found, false otherwise
};
