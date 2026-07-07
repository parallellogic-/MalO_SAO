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

void TransmitIR::_push_byte(uint8_t value)
{
  pulse_chain.append_note(255,127,16);
  pulse_chain.append_note(255,0,24+value);//max 40% duty cycle
}

void TransmitIR::_compress87(const char* in_arr,uint8_t* out_arr)
{ // Read 8 characters (7 bits each = 56 bits total) and pack into 7 bytes
  uint64_t out = 0;

  // 1. Pack 8 characters into a 64-bit integer (7 bits per char)
  for (uint8_t iter = 0; iter < 8; iter++) // Fixed: added iter++
  {
    out = (out << 7) | (in_arr[iter] & 0x7F);
  }

  // 2. Extract 7 bytes from the top down to keep correct string order
  uint8_t out_index=0;
  for (int8_t shift = 48; shift >= 0; shift -= 8) 
  {
    uint8_t tx = (out >> shift) & 0xFF;
    out_arr[out_index]=tx;
    out_index++;
  }
}

bool TransmitIR::push_message(const char* username,const char* message,uint8_t length)
{
  if(pulse_chain.is_busy()) return false;

  uint8_t compressed[DECODER_MAX_WS2812_MESSAGE_LENGTH]={};
  compressed[0]=length;//not currently used

  for(uint8_t iter=0;(iter*8)<USERNAME_MAX_LENGTH;iter++) _compress87(&username[iter*8],&compressed[1+iter*7]);
  for(uint8_t iter=0;(iter*8)<MESSAGE_MAX_LENGTH;iter++) _compress87(&message[iter*8],&compressed[1+USERNAME_MAX_LENGTH*7/8+iter*7]);

  RS::ReedSolomon<DECODER_MAX_WS2812_MESSAGE_LENGTH, RS_ECC_LENGTH> rs;
  uint8_t encoded[DECODER_MAX_WS2812_MESSAGE_LENGTH + RS_ECC_LENGTH];
  rs.Encode(compressed, encoded);

  //Serial.printf("yodel:\n");
  for(uint16_t iter=0;iter<sizeof(encoded)/sizeof(encoded[0]);iter++)
  {
    _push_byte(encoded[iter]);
    //if(iter>0 && iter%16==0) Serial.printf("\n");
    //Serial.printf("IR TxD byte yodel: %02X",encoded[iter]);
  }
  pulse_chain.append_note(255,127,16);//final pulse to allow receiver to get timing on the previous byte
  pulse_chain.append_note(255,0,1000);//>25 ms clearing at end of message to ensure receiver timeout

  pulse_chain.play();
  return true;
}

void TransmitIR::debug(uint32_t frame_id)
{
  if(frame_id%300!=0 || frame_id==0) return;
  //pulse_chain.debug(frame_id);
  const char username[USERNAME_MAX_LENGTH]="MalO_1234";
  const char message[MESSAGE_MAX_LENGTH]="Hello World";
  push_message(username,message,sizeof(message)/sizeof(message[0])+sizeof(username)/sizeof(username[0])+1);
}