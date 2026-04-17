#include "pio_diag.pio.h"

// --- Configuration ---
#define PIN_HIGH 0
#define PIN_LOW  7
// ---------------------

void setup() {
    PIO pio = pio0;
    uint sm = pio_claim_unused_sm(pio, true);
    uint offset = pio_add_program(pio, &drive_pins_program);

    // 1. Initialize pins for PIO
    pio_gpio_init(pio, PIN_HIGH);
    pio_gpio_init(pio, PIN_LOW);

    // 2. Force pins to OUTPUT using a bitmask (handles non-consecutive pins)
    uint32_t pin_mask = (1u << PIN_HIGH) | (1u << PIN_LOW);
    pio_sm_set_pindirs_with_mask(pio, sm, pin_mask, pin_mask);

    // 3. Configure PIO
    pio_sm_config c = drive_pins_program_get_default_config(offset);
    
    // Set 'out' base to 0 so 'out pins, 32' maps bit 0 to GPIO0, bit 1 to GPIO1, etc.
    sm_config_set_out_pins(&c, 0, 32); 
    
    pio_sm_init(pio, sm, offset, &c);
    pio_sm_set_enabled(pio, sm, true);

    // 4. Create and send the bitmask
    // We want PIN_HIGH to be 1, and PIN_LOW to be 0
    uint32_t status_mask = (1u << PIN_HIGH); 
    pio_sm_put_blocking(pio, sm, status_mask);
}

void loop() {
    // PIO is holding the pins in state
}