#include <hardware/pio.h>
#include <hardware/dma.h>
#include "charlie.pio.h"

// Define our 3 states using the format: [Duration(16b)][PinValues(8b)][PinDirs(8b)]
// Directions: 0xFF (all 8 pins as output)
// Pin Values: 0x01 (Pin 0 high), 0x02 (Pin 1 high), etc.
uint32_t state_red    = (6000000ULL << 16) | (0x01 << 8) | 0x03; 
uint32_t state_green  = (6000000ULL << 16) | (0x02 << 8) | 0x03;
uint32_t state_blue   = (6000000ULL << 16) | (0x04 << 8) | 0x06;

PIO pio = pio0;
uint sm;
int dma_chan;

void setup() {
    // 1. Initialize PIO
    uint offset = pio_add_program(pio, &charlie_dma_program);
    sm = pio_claim_unused_sm(pio, true);
    charlie_dma_program_init(pio, sm, offset, 2, 8); // Start at Pin 2, use 8 pins

    // 2. Claim and Configure DMA
    dma_chan = dma_claim_unused_channel(true);
    dma_channel_config c = dma_channel_get_default_config(dma_chan);
    
    channel_config_set_transfer_data_size(&c, DMA_SIZE_32);
    channel_config_set_read_increment(&c, false);  // Only reading one value at a time
    channel_config_set_write_increment(&c, false); // Writing to the same PIO FIFO
    channel_config_set_dreq(&c, pio_get_dreq(pio, sm, true)); // Sync with PIO speed

    dma_channel_configure(
        dma_chan,
        &c,
        &pio->txf[sm], // Destination: PIO TX FIFO
        NULL,          // Source: Set later in loop
        1,             // Transfer count: 1 32-bit word
        false          // Don't start yet
    );
}

void loop() {
    // Alternate between the three addresses
    
    // State 1: Red
    dma_channel_set_read_addr(dma_chan, &state_red, true);
    dma_channel_wait_for_finish_blocking(dma_chan);
    delay(100);
    
    // State 2: Green
    dma_channel_set_read_addr(dma_chan, &state_green, true);
    dma_channel_wait_for_finish_blocking(dma_chan);
    delay(100);
    
    // State 3: Blue
    dma_channel_set_read_addr(dma_chan, &state_blue, true);
    dma_channel_wait_for_finish_blocking(dma_chan);
    delay(400);
}