#include <Arduino.h>

// Force LVGL configuration overrides locally before pulling in the main header
#define LV_CONF_INCLUDE_SIMPLE
#include <lvgl.h>

// Your custom board's debug red LED pin assignment
#define PIN_DEBUG_R 37

void setup() {
  // 1. Instantly initialize the physical hardware debug pin
  gpio_init(PIN_DEBUG_R);
  gpio_set_dir(PIN_DEBUG_R, GPIO_OUT);
  
  // Turn the LED ON to signal that the test has commenced
  gpio_put(PIN_DEBUG_R, true);

  // 2. Open the UART serial pipeline
  Serial.begin(1000000);
  
  // Wait up to 3 seconds for the console monitor to bind
  uint32_t start_ms = millis();
  while (!Serial && (millis() - start_ms) < 3000);

  Serial.println("\n========================================");
  Serial.println("[RP2350B TEST] Starting Standalone execution pass...");
  Serial.println("[RP2350B TEST] Invoking lv_init()...");
  Serial.flush();
  
  // Introduce a slight pause to allow execution lines to stabilize
  delay(100);

  // 3. TARGET TEST FUNCTION
  // If the M33 instructions hard-fault, execution freezes right here.
  lv_init();

  // 4. VERIFICATION SUCCESS
  // If the processor reaches this line, lv_init() works perfectly on your chip.
  Serial.println("[RP2350B TEST] Success! lv_init() returned cleanly.");
  Serial.println("========================================");
  Serial.flush();

  // Turn the LED OFF to visually confirm the code bypassed the initialization lock
  gpio_put(PIN_DEBUG_R, false);
}

void loop() {
  // Rapidly toggle the LED if successful to provide continuous visual confirmation
  gpio_put(PIN_DEBUG_R, true);
  delay(100);
  gpio_put(PIN_DEBUG_R, false);
  delay(900);
}