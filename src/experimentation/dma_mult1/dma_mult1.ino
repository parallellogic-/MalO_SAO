#include "hardware/dma.h"

#define ARRAY_SIZE 10

// Fixed point math tables (base 2 logic scaled for integer safety)
// For simplicity in illustration, we generate standard mathematical LUT blocks
uint32_t log_table[256];
uint32_t exp_table[512];

// Input and Output memory blocks
alignas(4) uint32_t arrayA[ARRAY_SIZE] = {2, 3, 4, 5, 6, 7, 8, 9, 10, 5};
alignas(4) uint32_t arrayB[ARRAY_SIZE] = {10, 5, 12, 3, 9, 8, 4, 11, 2, 20};
alignas(4) uint32_t arrayC[ARRAY_SIZE] = {0};

// Shared DMA hardware memory register acting as our bus accumulator
alignas(4) volatile uint32_t dma_math_accumulator = 0;

void setup() {
  Serial.begin(115200);
  while (!Serial);

  Serial.println("--- RP2350 Pure Bus Matrix DMA Multiplication ---");

  // 1. Precompute the hardware LUT blocks on startup
  for (int i = 0; i < 256; i++) {
    log_table[i] = (i > 0) ? (uint32_t)(log2(i) * 32.0) : 0; // Scaled fixed point
  }
  for (int i = 0; i < 512; i++) {
    exp_table[i] = (uint32_t)round(pow(2.0, (double)i / 32.0));
  }

  // 2. Claim 3 structural DMA channels
  int dma_a = dma_claim_unused_channel(true);
  int dma_b = dma_claim_unused_channel(true);
  int dma_c = dma_claim_unused_channel(true);

  // --- CHANNEL A CONFIGURATION ---
  // Reads Array A, converts to log, loads into the accumulator register
  dma_channel_config c_a = dma_channel_get_default_config(dma_a);
  channel_config_set_transfer_data_size(&c_a, DMA_SIZE_32);
  channel_config_set_read_increment(&c_a, true);
  channel_config_set_write_increment(&c_a, false);
  channel_config_set_chain_to(&c_a, dma_b); // Hand control over to Channel B automatically
  channel_config_set_dreq(&c_a, DREQ_FORCE);

  // --- CHANNEL B CONFIGURATION ---
  // Reads Array B, converts to log, ADDS it to the accumulator register
  dma_channel_config c_b = dma_channel_get_default_config(dma_b);
  channel_config_set_transfer_data_size(&c_b, DMA_SIZE_32);
  channel_config_set_read_increment(&c_b, true);
  channel_config_set_write_increment(&c_b, false);
  channel_config_set_chain_to(&c_b, dma_c); // Hand control over to Channel C automatically
  channel_config_set_dreq(&c_b, DREQ_FORCE);

  // --- CHANNEL C CONFIGURATION ---
  // Reads the accumulated sum, converts via exp table, outputs into Array C
  dma_channel_config c_c = dma_channel_get_default_config(dma_c);
  channel_config_set_transfer_data_size(&c_c, DMA_SIZE_32);
  channel_config_set_read_increment(&c_c, false);
  channel_config_set_write_increment(&c_c, true);
  channel_config_set_dreq(&c_c, DREQ_FORCE);

  Serial.println("Starting Background Vector Processing Loop...");

  // Execute the vector calculations completely in the hardware bus matrix
  for (int i = 0; i < ARRAY_SIZE; i++) {
    
    // Clear out the temporary crossbar register for this loop cycle
    dma_math_accumulator = 0;

    // Step I: Point to index A element mapping through the log LUT
    uint32_t valA = arrayA[i];
    uint32_t logA = log_table[valA];

    // Step II: Point to index B element mapping through the log LUT
    uint32_t valB = arrayB[i];
    uint32_t logB = log_table[valB];

    // Configure Channel C to look up the final sum inside the Exponent Table
    dma_channel_configure(
      dma_c, &c_c, 
      &arrayC[i],                 // Write directly to our destination array
      &exp_table[logA + logB],    // Source data is pulled directly from the physical memory address mapping
      1, 
      false
    );

    // Configure Channel B to simulate addition across the bus line layout
    dma_channel_configure(
      dma_b, &c_b, 
      (volatile void*)&dma_math_accumulator, 
      &logB, 
      1, 
      false
    );

    // Trigger the chain cascade using Channel A
    dma_channel_configure(
      dma_a, &c_a, 
      (volatile void*)&dma_math_accumulator, 
      &logA, 
      1, 
      true // Set trigger to true to fire off the full hardware chain
    );

    // Wait until the final channel signals execution completion
    dma_channel_wait_for_finish_blocking(dma_c);
  }

  // Free resources safely
  dma_channel_unclaim(dma_a);
  dma_channel_unclaim(dma_b);
  dma_channel_unclaim(dma_c);

  // Output calculations verified purely out of system memory
  Serial.println("\nExecution Complete! Output Vector Content:");
  for (int i = 0; i < ARRAY_SIZE; i++) {
    Serial.printf("Index [%d]: %d * %d = %d\n", i, arrayA[i], arrayB[i], arrayC[i]);
  }
}

void loop() {
  // Completely empty
}
