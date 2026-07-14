#pragma once

#include "hardware/pio.h"
#include "logic_analyzer.pio.h"
#include "ir_txd.h" //constant definitions for packet structure

//shared class that handles both the ir_rxd, as well as ws2812 decode on gpio 1/2
//choose ws2812 protocol for IR Tx/rx so simplify code base (but with different time scales - 38/4 khz minimum lenght for a '1' on ir rx)
//0.08 mW/m2 is lower threshold? 

//IR issue expected to be shortening of '1' signal by ~25%, so target 42% and 88% duty cycle (?)

#define SHARED_BUFFER_LENGTH (256)//2 transitions, 8 bits = 16 bytes, assuming no actiivty on other channels.
//800 kHz * 2 1/0 transitions * 16.6 ms = 26560 --> 32768 ring buffer size (max size) if doing real WS2812 decode.  but that's for continuous stream of WS2812.
//Max should be 24 LEDs, *2 banks, *3 colors, *8 bits, *2 1/0 transitions = 2304 --> 4096 (PRECON: user sends max 1 frame of data per frame)
#define IR_RXD_PIN 43 //need to assign pull-up on IR RxD pin --> OBE, not needed
#define SHARED_BUFFER_FIRST_PIN 43
#define SHARED_BUFFER_PIN_COUNT 3 //IR_RXD, SAO_GP1, SAO_GP2
#define DECODER_MAX_GENERIC_MESSAGE_LENGTH (256*2*5/4) //256 characters, 2x 1/0 transitions, margin
#define DECODER_ACTIVITY_BRIGHTNESS (65535/10) //how long of 65535 should the PWM indicator be ON for each cycle (less is dimmer)
#define DECODER_TIMEOUT_US 10'000 //IR remote has 25 ms blanking between end of trnamissions and beginning of next one, so key off this.  need to be >7 ms to pick up on a 0xFF transmission

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
    uint16_t get_ring_buffer_index(bool is_read);//returns the index within the ring buffer where the latest reading was stored.  is_read for the latest addres that is valid to read from.  false to return the address where the DMA is about (but has not yet) written to
    const uint16_t get_buffer_length(){ return sizeof(_capture_buffer)/sizeof(_capture_buffer[0]); }
    void get_buffer_at(uint8_t pin_index,uint16_t sample_index,uint32_t &cycles,bool &value);//returns the number of 25 MHz cycles since the last state change (backward looking), and returns the new value the pin is now at after then transition (forward looking)
    bool is_activity(uint8_t pin_index,uint32_t timeout_us);//true if there has been any 1/0 or 0/1 transitions in the past timeout_us
};

enum DecodeState {
    STATE_LOOKING_FOR_START,
    STATE_MEASURING_HIGH,
    STATE_MEASURING_LOW
};

//key off the gap between IR messagesand then store the duration of the alernating 1 (38 khz present) and 0s (no IR present)
class DecoderGeneric{
  private:
    SharedDecoderBuffer *_buffer_ptr=nullptr;
    int16_t _ring_buffer_index=0;//location of last read from ring-buffer (or first observed transition)
    bool _is_ping_pong=false;
    uint32_t _decode_buffer[2][DECODER_MAX_GENERIC_MESSAGE_LENGTH]; //~32kB
    uint16_t _decode_index[2]={0,0};//index where next byte should be written to (aka 'length' when message is complete)
    bool _is_read_ready[2]={false,false};//flag asserted after the get_message() has returned the current message
    uint8_t _rxd_pin_index; //index of the pin within the pio_sm output to inspect inpnut from (0 to LOGIC_ANALYZER_PIN_COUNT-1)
    int8_t _pwm_activity_pin=-1; //where to route is_activity indication to during update()
    DecodeState _state = STATE_LOOKING_FOR_START;
    uint32_t _running_cycle_count = 0;
    uint32_t _timeout_us;//when a gap between messages is long enough to register as the start of a new message
  public:
    DecoderGeneric(uint8_t rxd_pin_index,int8_t pwm_activity_pin=-1,uint32_t timeout_us=DECODER_TIMEOUT_US);
    void begin(SharedDecoderBuffer* buffer);
    void update();
    void debug();
    //void end();

    const bool get_ping_pong(){ return !_is_ping_pong; } //where to read out of
    const uint16_t get_buffer_length(){ return sizeof(_decode_buffer[0])/sizeof(_decode_buffer[0][0]); }
    void set_activity(bool is_activity);//flush activity state to PWM LED
    const uint16_t get_message_length(){ return _decode_index[!_is_ping_pong]; }
    const uint32_t get_message_at(uint32_t index){ if(index>=_decode_index[!_is_ping_pong]) return 0; return _decode_buffer[!_is_ping_pong][index]; } //query the ping_pong buffer.  result is in _cycles (counts at 25 MHz)
    bool get_message(uint32_t *message, uint16_t &message_length);//make a deep copy of the message out fothe ping_pong buffer.  result is in _cycles (counts at 25 MHz
};

//make sense of the 1 and 0 durations from GenericDecoder
class DecoderWS2812{
  private:
    DecoderGeneric *_generic_decoder_ptr=nullptr;
    bool _is_ping_pong=false; // False (0) = buffer 0, True (1) = buffer 1
    //uint8_t _decode_buffer[2][DECODER_MAX_WS2812_MESSAGE_LENGTH];
    uint32_t _period=0;//emperically derived period (look at first byte received and look at most common period of 0->1 transitions and use that to decode)
    bool _is_read_ready[2] = {false, false}; // Flag asserted when a message is ready and waiting to be read

    void set_activity(bool is_activity);//flush activity state to PWM LED
    uint32_t _get_median_signal_period(); //helper to determine the spacing between bits
    float _get_exact_frequency(float base_frequency_hz,uint8_t period);//account for the round-off shift in the configuration timing.  38 vs 38.1 kHz loses lock after ~64 bytes
    void _decompress78(const uint8_t* in_arr,char* out_arr);//compress 8 characters into 7 bytes
  public:
    DecoderWS2812();
    void begin(DecoderGeneric* buffer);
    void debug();

    //const uint16_t get_buffer_length(){ return sizeof(_decode_buffer[0])/sizeof(_decode_buffer[0][0]); }
    bool get_message(bool is_ping_pong,char *username,char *message);//, uint16_t &message_length); //if message is found, store into &message (expect max 256 character size).  period_cycles is counts at 25 MHz (40 ns per count)
    bool get_message(char *username,char *message);//, uint16_t &message_length); //assumes single consumer
};


