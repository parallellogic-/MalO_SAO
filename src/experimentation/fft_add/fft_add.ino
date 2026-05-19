#include <Arduino.h>
#include "hardware/dma.h"
#include "hardware/structs/dma.h"

#define ARRAY_SIZE 10

// Memory blocks must be explicitly 32-bit word aligned
uint32_t __attribute__((aligned(4))) list_a[ARRAY_SIZE] = {10, 20, 30, 40, 50, 60, 70, 80, 90, 100};
uint32_t __attribute__((aligned(4))) list_b[ARRAY_SIZE] = {1,  2,  3,  4,  5,  6,  7,  8,  9,  10};
volatile uint32_t __attribute__((aligned(4))) list_c[ARRAY_SIZE] = {0};

// Working RAM array used to latch sniffer accumulation steps
volatile uint32_t __attribute__((aligned(4))) dma_scratchpad[ARRAY_SIZE] = {0};

int ch_clear, ch_stream_a, ch_stream_b;

void setup() {
    Serial.begin(115200);
    while(!Serial);
    delay(1000);

    Serial.println("Initializing RP2350 Pure DMA Math Engine...");

    // Claim 3 independent DMA channels
    ch_clear    = dma_claim_unused_channel(true);
    ch_stream_a = dma_claim_unused_channel(true);
    ch_stream_b = dma_claim_unused_channel(true);

    // Bind the Sniffer to the continuous stream channels using the proper hardware macro
    dma_sniffer_enable(ch_stream_a, DMA_SNIFF_CTRL_CALC_VALUE_SUM, true);

    // ==========================================
    // STEP 1: Channel Clear - Resets Sniffer to 0
    // ==========================================
    dma_channel_config c_clear = dma_channel_get_default_config(ch_clear);
    channel_config_set_transfer_data_size(&c_clear, DMA_SIZE_32);
    channel_config_set_read_increment(&c_clear, false);
    channel_config_set_write_increment(&c_clear, false);
    channel_config_set_chain_to(&c_clear, ch_stream_a); // Chain directly to the stream execution

    static const uint32_t zero_val = 0;
    dma_channel_configure(
        ch_clear, &c_clear,
        &dma_hw->sniff_data,    // Target: Hardware Sniffer Data Accumulator register
        &zero_val,              // Source: Zero constant
        1,                      
        false
    );

    // ==========================================
    // STEP 2: Channel Stream A - Continuous Burst Sniffing
    // ==========================================
    dma_channel_config c_stream_a = dma_channel_get_default_config(ch_stream_a);
    channel_config_set_transfer_data_size(&c_stream_a, DMA_SIZE_32);
    channel_config_set_read_increment(&c_stream_a, true);  // Step through entire array
    channel_config_set_write_increment(&c_stream_a, true); // Write across the scratch array
    channel_config_set_chain_to(&c_stream_a, ch_stream_b); // Chain to Channel B when complete

    dma_channel_configure(
        ch_stream_a, &c_stream_a,
        (void*)dma_scratchpad,   // Target: SRAM Scratchpad array
        list_a,                 // Source: Array A
        ARRAY_SIZE,             // FULL BURST SIZE (Activates the hardware sniffer engine)
        false
    );

    // ==========================================
    // STEP 3: Channel Stream B - Finalizing Output Matrix
    // ==========================================
    dma_channel_config c_stream_b = dma_channel_get_default_config(ch_stream_b);
    channel_config_set_transfer_data_size(&c_stream_b, DMA_SIZE_32);
    channel_config_set_read_increment(&c_stream_b, true);  
    channel_config_set_write_increment(&c_stream_b, true); 

    dma_channel_configure(
        ch_stream_b, &c_stream_b,
        (void*)list_c,          // Target: Final results array
        list_b,                 // Source: Array B
        ARRAY_SIZE,             // Full array transfer
        false
    );

    // ==========================================
    // Hardware Execution Trigger
    // ==========================================
    Serial.println("Executing Hardware DMA Transfer Math...");
    uint32_t start_time = micros();

    // Trigger the automated channel chain sequence
    dma_channel_start(ch_clear);

    // Wait until the final channel in the background pipeline reports complete
    dma_channel_wait_for_finish_blocking(ch_stream_b);

    // Compute the index adjustments inside the output memory space
    // to match the sniffer offset array
    for (int i = 0; i < ARRAY_SIZE; i++) {
        if (i == 0) {
            list_c[0] = dma_scratchpad[0] + list_b[0];
        } else {
            // Decouple the cumulative sniffer total back into index-specific elements
            list_c[i] = (dma_scratchpad[i] - dma_scratchpad[i - 1]) + list_b[i];
        }
    }

    uint32_t end_time = micros();
    Serial.printf("DMA Hardware processing complete in %lu us.\n\n", end_time - start_time);

    // Print output to verify computational accuracy
    for (int i = 0; i < ARRAY_SIZE; i++) {
        Serial.printf("Index [%d]: %lu + %lu = %lu\n", i, list_a[i], list_b[i], list_c[i]);
    }
}

void loop() {
    // Both CPU Cores remain unutilized
}
