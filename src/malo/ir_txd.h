#pragma once

#include "dma_control_block.h"
#include "pulse_chain.h"

#define TRANSMITTER_ACTIVITY_BRIGHTNESS (65535/10) //how long of 65535 should the PWM indicator be ON for each cycle (less is dimmer).  this is just the visible indicator LED, independent of the IR LED
#define IR_TXD_FREQUENCY_HZ 38'000.0f //lowest frequency the system generates (256 counts on period)
#define IR_TXD_PIN 16
#define USERNAME_MAX_LENGTH 16 //max numbers of letters in a username, there is no \0, will be compressed from 8-bit char to 7-bit value
#define MESSAGE_MAX_LENGTH 128 //max number of characters in a message, there is no \0, will be compressed from 8-bit char to 7-bit value
#define DECODER_MAX_WS2812_MESSAGE_LENGTH (1+USERNAME_MAX_LENGTH*7/8+MESSAGE_MAX_LENGTH*7/8) //256 characters, margin --> 1 length byte, 14 username, 126 message
#define RS_ECC_LENGTH (253-DECODER_MAX_WS2812_MESSAGE_LENGTH) //bytes used to correct corrupted bytes of the message

class TransmitIR {
private:
  PulseChain pulse_chain=PulseChain();
  int8_t _pwm_activity_pin=-1;
  float _baud_hz=3000.0; //3k baud, 256 characters, 8 bits/char = 0.7 seconds
  void _push_byte(uint8_t value);//push one byte "live" to transmitter (it's buffer queued, not immediate, until pulse_chain.play() is called)
  void _compress87(const char* in_arr,uint8_t* out_arr);//compress 8 characters into 7 bytes
public:
  TransmitIR(int8_t pwm_activity_pin=-1);
  void begin(PIOProgramManager &pio_program_manager,uint8_t pwm_pin=IR_TXD_PIN);
  void update();
  void end();

  bool is_busy(){ return pulse_chain.is_busy(); }
  bool push_message(const char* username,const char* message,uint8_t length); //PRECON: Message that is sent is 128 letters (pad with '\0' or spaces), username is 15 letters (pad with '\0' or spaces)
  void set_activity(bool is_activity);

  void debug(uint32_t frame_id);
};