#include "ir_txd.h"


TransmitIR::TransmitIR(int8_t pwm_activity_pin) :
  _pwm_activity_pin(pwm_activity_pin) {}

void TransmitIR::begin(PIOProgramManager &pio_program_manager,uint8_t pwm_pin)
{
  float base_frequency_hz=IR_TXD_FREQUENCY_HZ;
  pulse_chain.begin(pio_program_manager,pwm_pin,base_frequency_hz);

  if(_pwm_activity_pin>=0 && (gpio_get_dir(_pwm_activity_pin) == GPIO_IN))
  {//if activity indictor pins has NOT been claimed by application user, and it's a valid input, then route it as a PWM output
    // 1. Tell the GPIO mux to route the PWM peripheral to this pin
    gpio_set_function(_pwm_activity_pin, GPIO_FUNC_PWM);

    // 2. Find out which PWM slice and channel are connected to the pin
    uint slice_num = pwm_gpio_to_slice_num(_pwm_activity_pin);
    uint channel = pwm_gpio_to_channel(_pwm_activity_pin);

    // 3. Set the period/wrap value (e.g., 65535 for 16-bit resolution)
    pwm_set_wrap(slice_num, 65535);

    // 4. Set the duty cycle level to 0 (0% duty cycle)
    set_activity(0);

    // 5. Start the PWM slice hardware running
    pwm_set_enabled(slice_num, true);
  }
}

//show acitivty on led indictor, if any
void TransmitIR::set_activity(bool is_activity)
{
  if(_pwm_activity_pin<0) return; //invalid input, nothin to set
  uint slice_num = pwm_gpio_to_slice_num(_pwm_activity_pin);
  uint channel = pwm_gpio_to_channel(_pwm_activity_pin);
  pwm_set_chan_level(slice_num, channel, is_activity?TRANSMITTER_ACTIVITY_BRIGHTNESS:0);
}

void TransmitIR::update()
{
  //update status indicator
  set_activity(is_busy());
  Serial.printf("IR TxD busy: %d\n",is_busy());
}

void TransmitIR::end()
{

}

bool TransmitIR::push_message(uint8_t* message,uint16_t length)
{
  if(pulse_chain.is_busy()) return false;
  for(uint16_t iter=0;iter<length;iter++)
  {
    for(int8_t bit=7;bit>=0;bit--)
    {
      bool bit_to_send=message[iter]>>bit;//send most significant bit first
      uint8_t  period=255;
      uint8_t  duty=127;
      uint16_t cycle_count=bit_to_send?(2*IR_TXD_FREQUENCY_HZ/_baud_hz/3):(  IR_TXD_FREQUENCY_HZ/_baud_hz/3); // 2/3 ON for a 1 (1/3 ON for a 0)
      pulse_chain.append_note(period,duty,cycle_count);
               duty=0;
               cycle_count=bit_to_send?(  IR_TXD_FREQUENCY_HZ/_baud_hz/3):(2*IR_TXD_FREQUENCY_HZ/_baud_hz/3); // 2/3 OFF for a 1 (2/3 OFF for a 0)
      pulse_chain.append_note(period,duty,cycle_count);
    }
  }
  pulse_chain.play();
  return true;
}

void TransmitIR::debug(uint32_t frame_id)
{
    pulse_chain.debug(frame_id);
}