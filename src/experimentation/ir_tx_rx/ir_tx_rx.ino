/*
  Blink

  Turns an LED on for one second, then off for one second, repeatedly.

  Most Arduinos have an on-board LED you can control. On the UNO, MEGA and ZERO
  it is attached to digital pin 13, on MKR1000 on pin 6. LED_BUILTIN is set to
  the correct LED pin independent of which board is used.
  If you want to know what pin the on-board LED is connected to on your Arduino
  model, check the Technical Specs of your board at:
  https://docs.arduino.cc/hardware/

  modified 8 May 2014
  by Scott Fitzgerald
  modified 2 Sep 2016
  by Arturo Guadalupi
  modified 8 Sep 2016
  by Colby Newman

  This example code is in the public domain.

  https://docs.arduino.cc/built-in-examples/basics/Blink/
*/
#include "hardware/pwm.h"

const int IR_TX_INDICATOR_PIN=LED_BUILTIN;
const int IR_RX_INDICATOR_PIN=27;
const int IR_RX_PIN=20;//IRIn "In" indicator LED (baseband receipt)
const int IR_TX_PIN=21;//IROut "out" indicator LED (38khz transmit) https://www.adafruit.com/product/5990
uint slice_num;
long setting=150000000/38000;

// the setup function runs once when you press reset or power the board
void setup() {
  // initialize digital pin LED_BUILTIN as an output.
  pinMode(IR_TX_INDICATOR_PIN, OUTPUT);
  pinMode(IR_RX_INDICATOR_PIN, OUTPUT);
  pinMode(IR_RX_PIN, INPUT_PULLUP);
  
  // Configure pin for PWM
  gpio_set_function(IR_TX_PIN, GPIO_FUNC_PWM);
  slice_num = pwm_gpio_to_slice_num(IR_TX_PIN);

  // Set PWM frequency to 38kHz
  // Formula: F_pwm = F_clk / (prescaler * top)
  // 125MHz / (1 * 3289) = 38005 Hz
  // XXXX = 150000000/38000 Hz
  pwm_set_wrap(slice_num, setting); 
  pwm_set_chan_level(slice_num, pwm_gpio_to_channel(IR_TX_PIN), setting / 2); // 50% duty cycle
  pwm_set_enabled(slice_num, true);
}

// the loop function runs over and over again forever
void loop() {
  for(int is_high=0;is_high<2;is_high++)
  {
    digitalWrite(IR_TX_INDICATOR_PIN, is_high);  // change state of the LED by setting the pin to the HIGH voltage level
    if(is_high)
    {
      gpio_set_function(IR_TX_PIN, GPIO_FUNC_PWM);
      pwm_set_chan_level(slice_num, pwm_gpio_to_channel(IR_TX_PIN), setting / 2);
    }
    else
    {
      gpio_set_function(IR_TX_PIN, GPIO_FUNC_SIO);
      gpio_set_dir(IR_TX_PIN, GPIO_OUT);
      gpio_put(IR_TX_PIN, 0);
    }
    for(int iter=0;iter<100;iter++)
    {
      delay(10);
      bool is_rx=digitalRead(IR_RX_PIN);
      digitalWrite(IR_RX_INDICATOR_PIN,is_rx);
    }
  }
}
