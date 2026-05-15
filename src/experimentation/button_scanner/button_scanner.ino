#include "hardware/pio.h"
#include "hardware/dma.h"
#include "button_scanner.pio.h"

#define BASE_PIN 28
#define PIN_COUNT 9

volatile uint16_t dma_result = 0;
int dma_chan;

void setup() {
    Serial.begin(115200);
    while(!Serial);
    Serial.println("START");

    PIO pio = pio1;
    uint sm = 0;

    pio_set_gpio_base(pio, 16);

    for(int i = 0; i < PIN_COUNT; i++) {
        pio_gpio_init(pio, BASE_PIN + i);
        //gpio_pull_down(BASE_PIN + i); // Essential for floating inputs
    }

    uint offset = pio_add_program(pio, &button_scanner_program);
    pio_sm_config c = button_scanner_program_get_default_config(offset);
    
    sm_config_set_out_pins(&c, BASE_PIN-16, PIN_COUNT);
    sm_config_set_in_pins(&c, BASE_PIN-16);
    sm_config_set_in_pin_count(&c, PIN_COUNT);
    
    // Disable autopush for the ISR because we use the ISR for the delay calculation
    // We will manually push in the PIO code
    sm_config_set_in_shift(&c, false, false, 16); 

    pio_sm_init(pio, sm, offset, &c);
    pio_sm_set_enabled(pio, sm, true);

    // DMA Config
    dma_chan = dma_claim_unused_channel(true);
    dma_channel_config dc = dma_channel_get_default_config(dma_chan);
    channel_config_set_transfer_data_size(&dc, DMA_SIZE_16);
    channel_config_set_read_increment(&dc, false);
    channel_config_set_write_increment(&dc, false);
    channel_config_set_dreq(&dc, pio_get_dreq(pio, sm, false));

    dma_channel_configure(dma_chan, &dc, &dma_result, &pio->rxf[sm], 0xFFFFFFFF, true);
}

static uint16_t last_seen = 0;
void loop() {
    uint16_t current = dma_result & 0x1FF; 

    if (current != 0 && current != last_seen) {
        Serial.print("Pins [28-36]: ");
        for (int i = PIN_COUNT - 1; i >= 0; i--) {
            Serial.print((current >> i) & 1);
        }
        Serial.println();
    }
    last_seen = current;
    delay(5); 
}
