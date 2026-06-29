#pragma once

#include "touch.h"
#include "universal_serial_bus.h"
#include "dma_control_block.h"
#include "screen.h"
#include "led.h"
#include "imu.h"
#include "light_sensor.h"
#include "graphics.h"
#include "microphone.h"
#include <Wire.h>



struct SensorSuite{//bundle into an object to make easier to pass through graphics handling
  uint32_t frame_id;

  Graphics graphics;
  IMU imu;
  Charlieplex led_lower;
  Charlieplex led_upper;
  LightSensor light_sensor;
  Microphone microphone;
  ScatterGatherEngine scatterer_gatherer_engine_general;
  ScatterGatherEngine scatterer_gatherer_engine_screen;
  Screen screen;
  Touch touch;
};