#pragma once

#include "touch.h"
#include "pio_program_manager.h"
#include "universal_serial_bus.h"
#include "dma_control_block.h"
#include "screen.h"
#include "led.h"
#include "imu.h"
#include "light_sensor.h"
#include "graphics.h"
#include "microphone.h"
#include <Wire.h>
#include "ir_rxd.h"
#include "ir_txd.h"

// -- define --

#define PIN_DEBUG_R 37
#define PIN_DEBUG_G 38

#define I2C0_SDA 12 //todo: move to imu
#define I2C0_SCL 13
#define I2C0_BAUD 400'000


struct SensorSuite{//bundle into an object to make easier to pass through graphics handling
  uint32_t frame_id;

  Graphics graphics;
  IMU imu;
  Charlieplex led_lower;
  Charlieplex led_upper;
  DecoderGeneric decoder_ir_rxd;
  DecoderWS2812 decoder_ir_rxd_ws2812;
  LightSensor light_sensor;
  Microphone microphone;
  PIOProgramManager pio_charlieplex;
  PIOProgramManager pio_logic_analyzer;
  PIOProgramManager pio_addr;
  ScatterGatherEngine scatterer_gatherer_engine_general;
  ScatterGatherEngine scatterer_gatherer_engine_screen;
  Screen screen;
  SharedDecoderBuffer shared_decoder_buffer;
  Touch touch;
  TransmitIR ir_txd;
};