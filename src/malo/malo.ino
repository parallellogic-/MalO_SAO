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
#include "charlieplex.pio.h"
#include "logic_analyzer.pio.h"
#include "addr.pio.h"

// -- objects --

SensorSuite sensor_suite = {
  .frame_id=0xFFFFFFFF,

  //.graphics=Graphics(),
  .imu=IMU(),
  .led_lower=Charlieplex(0),
  .led_upper=Charlieplex(1),
  .decoder_ir_rxd=DecoderGeneric(0,PIN_DEBUG_R), //monitor the 0th pin, starting at the default offset (IR input pin).
  //.decoder_sao_gp1=DecoderGeneric(1), //FUTURE
  //.decoder_sao_gp1=DecoderGeneric(2), //Note: beware heavy usage of RAM for decode buffers - there is opportunity for RAM usage optimization with more advance WS2812 decode state machine
  .decoder_ir_rxd_ws2812=DecoderWS2812(),
  .light_sensor=LightSensor(),
  .microphone=Microphone(),
  .oled=OLED(),
  .pio_charlieplex=PIOProgramManager(pio0,&charlieplex_dma_program,0), //pio needs to be on lower bank to reach gp0.  duty cycle pairs of LEDs spread across 8 output pins
  .pio_logic_analyzer=PIOProgramManager(pio1,&logic_analyzer_program,16), //needs to be a separate pio to reach above pin 32 (configured at bank level.  run-length encoder of 11 pin states
  .pio_addr=PIOProgramManager(pio0,&pio_adder_program,0), //support for DMA to do simple address math like +1 and +2
  .scatterer_gatherer_engine_general=ScatterGatherEngine(),
  .scatterer_gatherer_engine_screen=ScatterGatherEngine(),
  .screen_manager=ScreenManager(),
  .shared_decoder_buffer=SharedDecoderBuffer(),
  .touch=Touch(),
  .ir_txd=TransmitIR(PIN_DEBUG_G) //indicate IR transmit activity on this debug LED
};

// -- variables --

volatile uint32_t frame_id0=0xFFFFFFFF;
uint64_t frame_us=0;
volatile bool setup0_complete=false;
void setup() {//core 0
  UniversalSerialBus::begin();
  pinMode(PIN_DEBUG_R,OUTPUT);//if unset, then ir rxd/txd will default to putting out pwm signals here to show ir status
  pinMode(PIN_DEBUG_G,OUTPUT);
  Serial.printf("UniversalSerialBus::begin DONE\n");

  sensor_suite.pio_charlieplex.begin();
  sensor_suite.pio_logic_analyzer.begin();
  sensor_suite.pio_addr.begin();
  Serial.printf("sensor_suite.shared_decoder_buffer.begin\n");
  sensor_suite.shared_decoder_buffer.begin(sensor_suite.pio_logic_analyzer);

  Serial.printf("sensor_suite.decoder_ir_rxd.begin\n");
  sensor_suite.decoder_ir_rxd.begin(&sensor_suite.shared_decoder_buffer);
  Serial.printf("sensor_suite.decoder_ir_rxd_ws2812.begin\n");
  sensor_suite.decoder_ir_rxd_ws2812.begin(&sensor_suite.decoder_ir_rxd);
  //sensor_suite.graphics.begin(sensor_suite);
  Serial.printf("sensor_suite.screen_manager.begin\n");
  sensor_suite.screen_manager.begin(sensor_suite);
  Serial.printf("sensor_suite.screen_manager.begin DONE\n");

  Wire.setSDA(I2C0_SDA);
  Wire.setSCL(I2C0_SCL);
  Wire.begin();
  Wire.setClock(I2C0_BAUD);
  sensor_suite.light_sensor.begin();
  sensor_suite.scatterer_gatherer_engine_general.registerSource(&sensor_suite.light_sensor);
  sensor_suite.imu.begin();
  sensor_suite.scatterer_gatherer_engine_general.registerSource(&sensor_suite.imu);//IMU after light sensor on shared I2C bus
  //TODO: RFID init here on same shared i2c bus...
  
  sensor_suite.microphone.begin();
  sensor_suite.scatterer_gatherer_engine_general.begin(true); //I2C needs aux channels to perform sync'd reads.  also uses sniff0 to compute the length of the imu fifo
  sensor_suite.scatterer_gatherer_engine_screen.begin(false); //limit to only 2 channels for screen
  sensor_suite.oled.begin();
  sensor_suite.scatterer_gatherer_engine_screen.registerSource(&sensor_suite.oled);
  sensor_suite.scatterer_gatherer_engine_general.registerSource(&sensor_suite.scatterer_gatherer_engine_general);
  sensor_suite.scatterer_gatherer_engine_screen.registerSource(&sensor_suite.scatterer_gatherer_engine_screen);//register self to perform end-of-cycle completion check
  sensor_suite.led_upper.begin(sensor_suite.pio_charlieplex);
  sensor_suite.led_lower.begin(sensor_suite.pio_charlieplex);
  sensor_suite.touch.begin(sensor_suite.pio_logic_analyzer);
  sensor_suite.ir_txd.begin(sensor_suite.pio_addr);
  //pinMode(VIBRATION_MOTOR_PIN,OUTPUT);

  setup0_complete=true;
  //Serial.println("SETUP0 DONE");
  frame_us=time_us_64();
}

volatile bool is_core1_shutdown_request=false; //core0 flag to core1 to begin shutdown
volatile bool is_core1_shutdown=false; //core1 flag to core0 that shutdown is complete
void loop() { //core 0
  digitalWrite(PIN_DEBUG_R,millis()%200>=100);
  //Serial.printf("core0 loop done: %d\n",sensor_suite.frame_id0);
  if(is_core1_shutdown && UniversalSerialBus::get_mounted())
  {//in usb mode, file system exposed only, all other activity silenced
    pinMode(PIN_DEBUG_R,OUTPUT);
    digitalWrite(PIN_DEBUG_R,millis()%200>=100);//activity indicator
    UniversalSerialBus::update(is_core1_shutdown);
    return;
  }//fast solo loop in usb mode
  
  busy_wait_until(frame_us+16666);//target 60 FPS, but allow clean recovery if something runs long
  frame_us=time_us_64();
  sensor_suite.frame_id++;
  frame_id0=sensor_suite.frame_id;
  rp2040.fifo.push_nb(frame_id0); //signal core1 to run

  if(UniversalSerialBus::get_mount_request()) is_core1_shutdown_request=true;//ensure graphics had a chance to push busy image to screen before asking core1 to shutdown
  UniversalSerialBus::update(is_core1_shutdown);
  if(!UniversalSerialBus::get_mounted())
  {
    sensor_suite.screen_manager.diag();
    sensor_suite.screen_manager.update();
    //if(sensor_suite.touch.get_down_button() && millis()>8000) UniversalSerialBus::set_mounted();
  }
  uint64_t end_us=time_us_64();
  Serial.printf("core0 runtime us: %u, %.2f%%\n",(uint32_t)(end_us-frame_us),(double)(end_us-frame_us)/166.6);
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

  do{
    frame_id1=rp2040.fifo.pop();
  }while(rp2040.fifo.available());
  
  uint64_t start_us=time_us_64();

  if(!is_core1_shutdown)
  {
    if(!is_core1_shutdown_request)
    {
      sensor_suite.imu.update(); //before scatterer-gather enginer resets buffers (does introduce some additional timing jitter on the scatterer-gatherers...)
      sensor_suite.scatterer_gatherer_engine_screen.compileAndRun(frame_id1);
      sensor_suite.scatterer_gatherer_engine_general.compileAndRun(frame_id1);
      sensor_suite.touch.update(frame_id1);//kicked off very near the beginning of the frame, normally it takes core0 notably longer to compute what to display on the screen
      sensor_suite.microphone.update();
      sensor_suite.decoder_ir_rxd.update();
      sensor_suite.ir_txd.update(); //status led of txd
      //digitalWrite(VIBRATION_MOTOR_PIN,ensor_suite.touch.get_down_button()>0);

      //sensor_suite.touch.debug();
      Serial.printf("imu_c: %.2f, fifo: %d, ",sensor_suite.imu.get_celsius(),sensor_suite.imu.get_fifo_sample_count());
      Serial.printf("mic: %.2f, touch: %d, ",sensor_suite.microphone.get_mean_square(),sensor_suite.touch.get_down_button());
      Serial.printf("accel: %0.2f, %0.2f, %0.2f, gyro: %0.2f, %0.2f, %0.2f, light: %d\n",sensor_suite.imu.get_accel(0),sensor_suite.imu.get_accel(1),sensor_suite.imu.get_accel(2),sensor_suite.imu.get_gyro(0),sensor_suite.imu.get_gyro(1),sensor_suite.imu.get_gyro(2),sensor_suite.light_sensor.getBrightness());
      //sensor_suite.decoder_ir_rxd.debug();
      sensor_suite.decoder_ir_rxd_ws2812.debug();
      sensor_suite.ir_txd.debug(frame_id1);

    }else{
      sensor_suite.screen_manager.end();
      sensor_suite.scatterer_gatherer_engine_screen.end();
      sensor_suite.scatterer_gatherer_engine_general.end();
      sensor_suite.touch.end();
      sensor_suite.microphone.end();
      sensor_suite.imu.end();
      sensor_suite.shared_decoder_buffer.end();

      sensor_suite.pio_charlieplex.end();
      sensor_suite.pio_logic_analyzer.end();
    }
  }
  if(!is_core1_shutdown && is_core1_shutdown_request)
  {
    is_core1_shutdown=true;
    Serial.println("core1 DONE");
  }
  uint64_t end_us=time_us_64();
  Serial.printf("core1 runtime us: %u, %.2f%%, touch: %d\n",(uint32_t)(end_us-start_us),(double)(end_us-start_us)/166.6f,sensor_suite.touch.get_down_button());
}
