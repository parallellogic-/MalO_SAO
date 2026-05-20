#include "dma_control_block.h"
#include "light_sensor.h"

ScatterGatherEngine scatterer_gatherer_engine;
LightSensor light_sensor(i2c0);

#define I2C0_SDA 12
#define I2C0_SCL 13

uint32_t frame_id=0;

void setup() {

  Serial.begin();
  delay(2000);
  Serial.println("START");

  //restart_all_rp2350_resources();//still starts on frame 8

  //I2C patch for mis-routed pin to imu on prototype
  pinMode(15, OUTPUT);
  digitalWrite(15, HIGH);

  // put your setup code here, to run once:
  scatterer_gatherer_engine.begin();
  light_sensor.begin(I2C0_SDA,I2C0_SCL,400'000);
  scatterer_gatherer_engine.registerSource(&light_sensor);

  frame_id=0;
  Serial.println("DONE setup");
}

void loop() {
  uint32_t brightness=light_sensor.getBrightness();
  Serial.println(brightness);

  // put your main code here, to run repeatedly:
  scatterer_gatherer_engine.compileAndRun(frame_id++,0,0);
  delay(16);
}
