#include <hardware/pio.h>
#include <Adafruit_TinyUSB.h>
#include "addr.pio.h"

PIO pio = pio0;
uint sm = 0;
uint offset = 0;

void pio_adder_init(PIO pio_instance, uint sm_index, uint program_offset) {
    pio_sm_config c = pio_adder_program_get_default_config(program_offset);

    // Setup shift settings for perfect streaming
    sm_config_set_in_shift(&c, true, false, 32);  // Autopush OFF
    sm_config_set_out_shift(&c, true, false, 32); // Autopull OFF

    pio_sm_init(pio_instance, sm_index, program_offset, &c);
    pio_sm_set_enabled(pio_instance, sm_index, true);
}

uint32_t run_pio_adder(uint32_t input_val, uint32_t add_amount) {
    // Pushing data securely using blocking queues
    Serial.println("C");
    pio_sm_put_blocking(pio, sm, input_val);
    
    Serial.println("D");
    pio_sm_put_blocking(pio, sm, add_amount);

    // This will now unblock instantly and return 501 and 502 perfectly!
    Serial.println("A");
    uint32_t out=pio_sm_get_blocking(pio, sm);
    Serial.println("B");
    return out;
}

void setup() {
  Serial.begin(115200);
  while (!Serial) { delay(10); } 
  
  Serial.println("--- RP2350 PIO Adder Initialization ---");
  offset = pio_add_program(pio, &pio_adder_program);
  pio_adder_init(pio, sm, offset);
  Serial.println("PIO Hardware Engine Running Setup Complete.");
}

void loop() {
  Serial.println("loop");
  uint32_t base_value = millis()/1200;
  
  uint32_t res1 = run_pio_adder(base_value, 1);
  Serial.print("Input: "); Serial.print(base_value);
  Serial.print(" + 1 = "); Serial.println(res1);

  uint32_t res2 = run_pio_adder(base_value, 2);
  Serial.print("Input: "); Serial.print(base_value);
  Serial.print(" + 2 = "); Serial.println(res2);

  Serial.println("-------------------------------------");
  delay(300); 
  Serial.println("end_loop");
}
