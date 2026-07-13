#pragma once

#include "touch.h"
#include "pio_program_manager.h"
#include "universal_serial_bus_flash.h"
#include "dma_control_block.h"
#include "oled.h"
#include "led.h"
#include "imu.h"
#include "light_sensor.h"
//#include "graphics.h"
#include "screen_manager.h"
#include "microphone.h"
#include <Wire.h>
#include "ir_rxd.h"
#include "ir_txd.h"
#include "analog.h"

// -- define --

#define PIN_DEBUG_R 37
#define PIN_DEBUG_G 38

#define I2C0_SDA 12 //todo: move to imu
#define I2C0_SCL 13
#define I2C0_BAUD 400'000

#define VIBRATION_MOTOR_PIN 39

struct SensorSuite{//bundle into an object to make easier to pass through graphics handling
  uint32_t frame_id;
  uint32_t core0_frame_us; //frame generation time in microsectons
  uint32_t core1_frame_us;
  float lvgl_memory_percent;

  //Graphics graphics;
  Analog analog;
  IMU imu;
  Charlieplex led_lower;
  Charlieplex led_upper;
  DecoderGeneric decoder_ir_rxd;
  DecoderWS2812 decoder_ir_rxd_ws2812;
  LightSensor light_sensor;
  Microphone microphone;
  OLED oled;
  PIOProgramManager pio_charlieplex;
  PIOProgramManager pio_logic_analyzer;
  PIOProgramManager pio_addr;
  SaveState save_state;
  ScatterGatherEngine scatterer_gatherer_engine_general;
  ScatterGatherEngine scatterer_gatherer_engine_screen;
  ScreenManager screen_manager;
  SharedDecoderBuffer shared_decoder_buffer;
  Touch touch;
  TransmitIR ir_txd;
};