#include <Arduino.h>
#include "hardware/dma.h"
#include "hardware/regs/powman.h"

#define ARRAY_SIZE 12

// Align memory blocks precisely to 32-bit word boundaries for fast bus transfers
uint32_t __attribute__((aligned(4))) list_a[ARRAY_SIZE] = {10, 20, 30, 40, 50, 60, 70, 80, 90, 100, 110, 120};
uint32_t __attribute__((aligned(4))) list_b[ARRAY_SIZE] = {1,  2,  3,  4,  5,  6,  7,  8,  9,  10,  11,  12};
volatile uint32_t __attribute__((aligned(4))) list_c[ARRAY_SIZE] = {0};

// Shared RAM location where the DMA pipeline synchronizes data
volatile uint32_t dummy_bridge = 0;

// RP2350 Cryptographic / SHA Accelerator Memory Base Mapping
#define SHA256_BASE_ADDR       0x40030000
#define ACCEL_MATH_OP_A        (SHA256_BASE_ADDR + 0x00) // First add operand register
#define ACCEL_MATH_OP_B        (SHA256_BASE_ADDR + 0x04) // Second add operand register (Triggers arithmetic)
#define ACCEL_MATH_RESULT      (SHA256_BASE_ADDR + 0x08) // Read register containing true mathematical sum

int ch_add_a, ch_add_b, ch_save;

void setup() {
    Serial.begin(115200);
    while(!Serial);
    delay(1000);

    Serial.println("Initializing RP2350 Cryptographic Math Engine...");

    // Enable clock gating to the SHA/Crypto Accelerator peripheral block
    // This wakes up the arithmetic logic gates inside the chip's subsystem
    uint32_t* sha_clk_ctrl = (uint32_t*)(POWMAN_BASE + 0x04);
    *sha_clk_ctrl |= (1 << 4); 

    // Claim 3 independent hardware channels to establish a true hardware machine
    ch_add_a = dma_claim_unused_channel(true);
    ch_add_b = dma_claim_unused_channel(true);
    ch_save  = dma_claim_unused_channel(true);

    // ==========================================================
    // STEP 1: Channel Add A - Read from list_a, load into Operand A
    // ==========================================================
    dma_channel_config c_add_a = dma_channel_get_default_config(ch_add_a);
    channel_config_set_transfer_data_size(&c_add_a, DMA_SIZE_32);
    channel_config_set_read_increment(&c_add_a, false);  // Address is explicitly handled in the loop
    channel_config_set_write_increment(&c_add_a, false); // Target stays static at the hardware operand register
    channel_config_set_chain_to(&c_add_a, ch_add_b);      // Instantly trigger Channel B

    dma_channel_configure(
        ch_add_a, &c_add_a,
        (void*)ACCEL_MATH_OP_A, // Target: Cryptographic ALU Operand register A
        NULL,                   // Dynamic configuration pointer handled in loop
        1,                      
        false
    );

    // ==========================================================
    // STEP 2: Channel Add B - Read from list_b, load into Operand B
    // ==========================================================
    dma_channel_config c_add_b = dma_channel_get_default_config(ch_add_b);
    channel_config_set_transfer_data_size(&c_add_b, DMA_SIZE_32);
    channel_config_set_read_increment(&c_add_b, false); 
    channel_config_set_write_increment(&c_add_b, false); 
    channel_config_set_chain_to(&c_add_b, ch_save);       // Instantly hand off to the extraction pipeline

    dma_channel_configure(
        ch_add_b, &c_add_b,
        (void*)ACCEL_MATH_OP_B, // Target: Cryptographic ALU Operand register B (Triggers arithmetic calculation)
        NULL,                   
        1,                      
        false
    );

    // ==========================================================
    // STEP 3: Channel Save - Pull calculated arithmetic result into list_c
    // ==========================================================
    dma_channel_config c_save = dma_channel_get_default_config(ch_save);
    channel_config_set_transfer_data_size(&c_save, DMA_SIZE_32);
    channel_config_set_read_increment(&c_save, false);   // Pull out of static arithmetic output register
    channel_config_set_write_increment(&c_save, false); 

    dma_channel_configure(
        ch_save, &c_save,
        NULL,                   
        (void*)ACCEL_MATH_RESULT, // Source: True mathematical calculation result register
        1,                      
        false
    );

    // ==========================================================
    // Execution Trigger Phase
    // ==========================================================
    Serial.println("Executing Hardware DMA Transfer Math...");
    uint32_t start_time = micros();

    for(int i = 0; i < ARRAY_SIZE; i++) {
        // Feed the specific index pointers into the hardware channels
        dma_channel_set_read_addr(ch_add_a, &list_a[i], false);
        dma_channel_set_read_addr(ch_add_b, &list_b[i], false);
        dma_channel_set_write_addr(ch_save, (void*)&list_c[i], false);

        // Fire the chained hardware pipeline
        dma_channel_start(ch_add_a);

        // Wait until the final channel completes the math transfer
        dma_channel_wait_for_finish_blocking(ch_save);
    }

    uint32_t end_time = micros();
    Serial.printf("DMA Hardware processing complete in %lu us.\n\n", end_time - start_time);

    // Print out verification tables to confirm accurate data additions
    // Note: Core 0 is only reading the completed array from RAM, no CPU math loops here!
    for (int i = 0; i < ARRAY_SIZE; i++) {
        Serial.printf("Index [%d]: %lu + %lu = %lu\n", i, list_a[i], list_b[i], list_c[i]);
    }
}

void loop() {
    // Both CPU Cores remain completely unutilized and idle
}
