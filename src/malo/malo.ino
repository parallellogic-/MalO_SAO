// -- periperals --
//analog
//  hall effect sensor
//  internal temperature
//  potentiometer
//  voltage reference
//buzzer
//charlieplex_led
//capacitive touch
//debug leds
//SAO i2c1
//shared sensors i2c0
//  light sensor
//  rfid
//  imu, temperature
//ir
//  txd
//  rxd, gpio 1/2
//microphone
//oled screen spi1
//  graphics
//usb
//vibration motor, eccentric rotating mass

// -- key interactions --
//magnet unlock
//temperature, light trending
//input voltage monitoring
//message alerts (audio, vibration)
//8-bit led control
//button press inputs
//debug leds (ir rxd/txd)
//sao ws2812 decode
//sao memory map exposed
//rfid url exposed
//screen savers - pot for brightness control

// -- include --

#include "touch.h"
#include "logic_analyzer.pio.h" //todo: move to touch.c and ir_rxd


// -- define --

#define PIN_DEBUG_R 37
#define PIN_DEBUG_G 38


// -- objects --

Touch touch(pio1);


// -- variables --



void setup() {//core 0
  Serial.begin();//TODO: move to USB
  long start_tms=millis();
  while(!Serial && (millis()-start_tms)<7000);//wait for terminal to connect or timeout, whichever is first
  Serial.println("START");

  pio_set_gpio_base(pio1, 16);//todo: move into touch.c
  uint logic_analyzer_pio1_offset=pio_add_program(pio1, &logic_analyzer_program);
  touch.begin(logic_analyzer_pio1_offset);

  pinMode(PIN_DEBUG_R,OUTPUT);
}

void setup1(){ //core 1

}

uint64_t last_loop=0;
void loop() { //core 0
  while(time_us_64()-last_loop<16666) tight_loop_contents();
  last_loop=time_us_64();

  touch.update();

  digitalWrite(PIN_DEBUG_R,millis()%200<100);
  for(int iter=1;iter<11;iter++)
  {
    Serial.printf("%2d/%5d ",iter,touch.get_capacitive_touch(iter));
  }
  Serial.println();
}

void loop1(){ //core 1

}
