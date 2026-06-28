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

#include "malo.h"

// -- define --

#define PIN_DEBUG_R 37
#define PIN_DEBUG_G 38

// -- objects --

SensorSuite sensor_suite = {
  .frame_id=0xFFFFFFFF,

  .led_lower=Charlieplex(0),
  .led_upper=Charlieplex(1),
  .graphics=Graphics(),
  .scatterer_gatherer_engine_screen=ScatterGatherEngine(),
  .screen=Screen(),
  .touch=Touch(pio1)
};

// -- debug --


void update_led()
{
    sensor_suite.led_upper.set_max_effective_led_count(CHARLIPLEX_LED_COUNT/2);
    sensor_suite.led_lower.set_max_effective_led_count(CHARLIPLEX_LED_COUNT/2);

    for(uint8_t iter=0;iter<CHARLIPLEX_LED_COUNT;iter++)
    {
      //slow fade
      uint16_t brightness_upper = (-millis()/8)+iter*32; 
      if(brightness_upper & 0x0100) brightness_upper=255-(uint8_t)brightness_upper;//fade fully off half the time
      sensor_suite.led_upper.set_brightness(iter,(uint8_t)brightness_upper);
      sensor_suite.led_lower.set_brightness(iter,(uint8_t)brightness_upper);
    }
    sensor_suite.led_upper.flush();
    sensor_suite.led_lower.flush();
}


// -- variables --

volatile uint32_t frame_id0=0xFFFFFFFF;
uint64_t frame_us=0;
volatile bool setup0_complete=false;
void setup() {//core 0
  UniversalSerialBus::begin();
  pinMode(PIN_DEBUG_R,OUTPUT);//if unset, then ir rxd/txd will default to putting out pwm signals here to show ir status
  pinMode(PIN_DEBUG_G,OUTPUT);
  sensor_suite.graphics.begin(sensor_suite); //beware lvgl interaction with USB mass storage mode (?) also with touch (?)
  sensor_suite.scatterer_gatherer_engine_screen.begin(false); //limit to only 2 channels for screen
  sensor_suite.screen.begin();
  sensor_suite.scatterer_gatherer_engine_screen.registerSource(&sensor_suite.screen);
  sensor_suite.scatterer_gatherer_engine_screen.registerSource(&sensor_suite.scatterer_gatherer_engine_screen);//register self to perform end-of-cycle completion check
  sensor_suite.led_upper.begin();
  sensor_suite.led_lower.begin();
  sensor_suite.touch.begin();
  setup0_complete=true;
  //Serial.println("SETUP0 DONE");
  frame_us=time_us_64();
}

volatile bool is_core1_shutdown_request=false; //core0 flag to core1 to begin shutdown
volatile bool is_core1_shutdown=false; //core1 flag to core0 that shutdown is complete
void loop() { //core 0
  digitalWrite(PIN_DEBUG_R,millis()%200>=100);
  //Serial.printf("core0 loop done: %d\n",sensor_suite.frame_id0);
  //while(time_us_64()-frame_us<16666) yield();
  busy_wait_until(frame_us+16666);//target 60 FPS, but allow clean recovery if something runs long
  frame_us=time_us_64();
  sensor_suite.frame_id++;
  frame_id0=sensor_suite.frame_id;
  rp2040.fifo.push_nb(frame_id0); //signal core1 to run

  if(UniversalSerialBus::get_mount_request()) is_core1_shutdown_request=true;//ensure graphics had a chance to push busy image to screen before asking core1 to shutdown
  UniversalSerialBus::update(is_core1_shutdown);
  bool is_mounted=UniversalSerialBus::get_mounted();
  if(!is_mounted)
  {
    sensor_suite.graphics.update();
  }
  uint64_t end_us=time_us_64();
  Serial.printf("core0 runtime us: %d, %.2f%%\n",(uint32_t)(end_us-frame_us),(float)(end_us-frame_us)/166.6);
}

void __not_in_flash_func(setup1()){ //core 1
  delay(1);
  while(!setup0_complete) delay(1);//yield();
  //delay(17);
}

volatile uint32_t frame_id1=0xFFFFFFFF;
void __not_in_flash_func(loop1)(){ //core 1
  digitalWrite(PIN_DEBUG_G,millis()%200<100);

//  Serial.printf("core1 loop done: %d, %d\n",sensor_suite.frame_id1,sensor_suite.touch.get_down_button());

  //while(sensor_suite.frame_id0==sensor_suite.frame_id1) yield();
  //Serial.println("HERE1");
  //uint32_t token_value = 0;

  // Returns true if a value was pulled successfully, false if empty
  //while (frame_id1==frame_id0) delay(1);//yield();
  //frame_id1 = frame_id0;
      // Run frame code...
  do{
    frame_id1=rp2040.fifo.pop();
  }while(rp2040.fifo.available());
  //uint32_t token = rp2040.fifo.pop();//wait for core0 signal to run sensor update step
  //Serial.println("HERE2");
  //sensor_suite.frame_id1=sensor_suite.frame_id0;
  //delay(1);
  uint64_t start_us=time_us_64();

  if(!is_core1_shutdown)
  {
    if(!is_core1_shutdown_request)
    {
      // -- temp debug led --
      update_led();

      sensor_suite.scatterer_gatherer_engine_screen.compileAndRun(frame_id1);
      sensor_suite.touch.update(frame_id1);//kicked off very near the beginning of the frame, normally it takes core0 notably longer to compute what to display on the screen
      //sensor_suite.touch.debug();
    }else{
      sensor_suite.touch.end();
    }
  }
  if(!is_core1_shutdown && is_core1_shutdown_request)
  {
    is_core1_shutdown=true;
    Serial.println("core1 DONE");
  }
  uint64_t end_us=time_us_64();
  Serial.printf("core1 runtime us: %d, %.2f%%, touch: %d\n",(uint32_t)(end_us-start_us),(float)(end_us-start_us)/166.6,sensor_suite.touch.get_down_button());
}
