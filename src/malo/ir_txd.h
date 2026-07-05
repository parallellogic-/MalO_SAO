#pragma once

#include "dma_control_block.h"
#include "pulse_chain.h"

#define TRANSMITTER_ACTIVITY_BRIGHTNESS (65535/10) //how long of 65535 should the PWM indicator be ON for each cycle (less is dimmer).  this is just thevisible indicator LED, independent of the IR LED
#define IR_TXD_FREQUENCY_HZ 38'000.0f //lowest frequency the system generates (256 counts on period)
#define IR_TXD_PIN 16

class TransmitIR {
private:
  PulseChain pulse_chain=PulseChain();
  int8_t _pwm_activity_pin=-1;
  float _baud_hz=3000.0; //3k baud, 256 characters, 8 bits/char = 0.7 seconds
public:
  TransmitIR(int8_t pwm_activity_pin=-1);
  void begin(PIOProgramManager &pio_program_manager,uint8_t pwm_pin=IR_TXD_PIN);
  void update();
  void end();

  bool is_busy(){ return pulse_chain.is_busy(); }
  bool push_message(uint8_t* message,uint16_t length);
  void set_activity(bool is_activity);

  void debug(uint32_t frame_id);
};