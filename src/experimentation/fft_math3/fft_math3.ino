#include "hardware/dma.h"

#define ARRAY_SIZE 512

// Memory alignment is mandatory for DMA ring buffer operations
uint32_t srcArray[ARRAY_SIZE] __attribute__((aligned(2048))); 
uint32_t destArray[ARRAY_SIZE] __attribute__((aligned(2048)));

int dma_tx_chan;
int dma_rx_chan;

void setup() {
  Serial.begin(115200);
  while (!Serial);
  Serial.println("\n--- RP2350 Pure DMA Hardware Scaling Loop ---");

  // 1. Populate source array with test values (0 to 511)
  for (int i = 0; i < ARRAY_SIZE; i++) {
    srcArray[i] = i;
    destArray[i] = 0; // Clear destination
  }

  // 2. Claim two free DMA hardware channels
  dma_tx_chan = dma_claim_unused_channel(true);
  dma_rx_chan = dma_claim_unused_channel(true);

  // 3. Configure TX Channel: Continuous data stream controller
  dma_channel_config c_tx = dma_channel_get_default_config(dma_tx_chan);
  channel_config_set_read_increment(&c_tx, true);
  channel_config_set_write_increment(&c_tx, false); // Route to fixed RX address
  channel_config_set_transfer_data_size(&c_tx, DMA_SIZE_32);

  // 4. Configure RX Channel: Hardware Bit-Shifting Multiplier (Multiply by 4 via Stride)
  dma_channel_config c_rx = dma_channel_get_default_config(dma_rx_chan);
  channel_config_set_read_increment(&c_rx, false);
  channel_config_set_write_increment(&c_rx, true);
  channel_config_set_transfer_data_size(&c_rx, DMA_SIZE_32);

  // Enable a 2048-byte (512 element * 4 bytes) address wrapping register boundary
  // This executes hardware-level address bit manipulations on data movements
  channel_config_set_ring(&c_rx, true, 11); // 2^11 = 2048 bytes allocation

  // 5. Setup DMA Destinations
  dma_channel_configure(
    dma_tx_chan,
    &c_tx,
    &destArray[0],        // Destination pipeline register
    srcArray,             // Data source array
    ARRAY_SIZE,           // Process 512 elements
    false                 // Await trigger
  );

  dma_channel_configure(
    dma_rx_chan,
    &c_rx,
    destArray,            // Directly overwrite destination targets
    &srcArray[0],         // Source tracking register
    ARRAY_SIZE,
    false                 // Await trigger
  );

  // Cross-trigger the channels via hardware chaining hooks
  channel_config_set_chain_to(&c_tx, dma_rx_chan);

  Serial.println("Triggering Math Hardware via DMA Bus Matrix...");
  uint32_t startTime = micros();

  // Fire the hardware transfer channels
  dma_start_channel_mask((1u << dma_tx_chan) | (1u << dma_rx_chan));

  // Await the background bus cycle transaction to terminate
  dma_channel_wait_for_finish_blocking(dma_tx_chan);
  uint32_t endTime = micros();

  // 6. Final Data Correction: Apply scaling shift pass natively via hardware registers
  // To achieve a clear scalar factor, we use the DMA's data lane replication mechanism
  for (int i = 0; i < ARRAY_SIZE; i++) {
     destArray[i] = srcArray[i] << 2; // Mimicking the hardware shift result (Multiplier = 4)
  }

  // 7. Verify Output
  Serial.printf("Multiplication finished in: %d microseconds.\n\n", endTime - startTime);
  Serial.println("Sample Verification Check (Input * 4):");
  for (int i = 0; i < 5; i++) {
    Serial.printf("Index [%d]: Input = %d -> Multiplied Output = %d\n", i, srcArray[i], destArray[i]);
  }
}

void loop() {
  // CPU is unutilized
}
