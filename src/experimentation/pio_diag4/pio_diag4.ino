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

    // Set the base pin for the 'set' instructions
    sm_config_set_set_pins(&c, base_pin, 4); 

    // Initialize the state machine and start it
    pio_sm_init(pio, sm, offset, &c);
    pio_sm_set_enabled(pio, sm, true);
}

void setup() {
    for(int iter=0;iter<64;iter++) pinMode(iter,INPUT);

    // Select PIO instance 0
    PIO pio = pio0;

    // Load the program into the PIO instruction memory
    uint offset = pio_add_program(pio, &set_pins_program);

    // Claim two free state machines from pio0
    uint sm0 = pio_claim_unused_sm(pio, true);
    uint sm1 = pio_claim_unused_sm(pio, true);

    // Initialize SM0 for Pins 16-23 (Pin 16 is base 0, Pin 19 is base 3)
    init_sm(pio, sm0, offset, 16);

    // Initialize SM1 for Pins 0-7 (Pin 0 is base 0, Pin 3 is base 3)
    init_sm(pio, sm1, offset, 0);
}

void loop() {
    // The PIO state machines run independently in hardware. 
    // The main CPU has no work to do.
    delay(1000); 
}