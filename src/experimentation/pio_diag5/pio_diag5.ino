#include <Arduino.h>
#include "hardware/pio.h"
#include "set_pins.pio.h"

// Helper function to configure and initialize a state machine
void init_sm(PIO pio, uint sm, uint offset, uint base_pin) {
    // Configure all 8 pins in the group as PIO functions
    for (int i = 0; i < 8; i++) {
        pio_gpio_init(pio, base_pin + i);
    }

    pio_sm_config c = set_pins_program_get_default_config(offset);

    // FIX 1: Set out pins because 'mov pins' and 'mov pindirs' use the OUT pin mapping
    sm_config_set_out_pins(&c, base_pin, 8); 
    
    // Keep this if you also intend to use 'set' instructions elsewhere
//    sm_config_set_set_pins(&c, base_pin, 4); 

    // Initialize the state machine registers
    pio_sm_init(pio, sm, offset, &c);

    // FIX 2: Explicitly tell the PIO hardware that it is allowed to drive these 8 pins as outputs
    // Without this, 'mov pindirs' updates internal SM state but can't override the physical pin pads
    pio_sm_set_consecutive_pindirs(pio, sm, base_pin, 8, true);

    // Start it
    pio_sm_set_enabled(pio, sm, true);
}

void setup() {
    // Set all pins to input to clear weak pull-ups/pull-downs
    for(int iter=0; iter<64; iter++) {
        pinMode(iter, INPUT);
    }

    // Select PIO instance 0
    PIO pio = pio0;

    // Load the program into the PIO instruction memory
    uint offset = pio_add_program(pio, &set_pins_program);

    // Claim two free state machines from pio0
    uint sm0 = pio_claim_unused_sm(pio, true);
    uint sm1 = pio_claim_unused_sm(pio, true);

    // Initialize SM0 for Pins 16-23
    init_sm(pio, sm0, offset, 16);

    // Initialize SM1 for Pins 0-7
    init_sm(pio, sm1, offset, 0);
}

void loop() {
    // PIO runs independently in the background
    delay(1000); 
}
