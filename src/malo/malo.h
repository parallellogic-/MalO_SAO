#pragma once

#include "touch.h"
#include "universal_serial_bus.h"
#include "dma_control_block.h"
#include "screen.h"
#include "led.h"
#include "graphics.h"

struct SensorSuite{//bundle into an object to make easier to pass through graphics handling
  volatile uint32_t frame_id0;
  volatile uint32_t frame_id1;

  Charlieplex led_lower;
  Charlieplex led_upper;
  Graphics graphics;
  ScatterGatherEngine scatterer_gatherer_engine_screen;
  Screen screen;
  Touch touch;
};