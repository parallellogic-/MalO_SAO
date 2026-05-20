#include "dma_control_block.h"
#include "light_sensor.h"

ScatterGatherEngine scatterer_gatherer_engine;
LightSensor light_sensor(i2c0);

#define I2C0_SDA 12
#define I2C0_SCL 13

uint32_t frame_id=0;

void restart_all_rp2350_resources() {
    // === STAGE 1: FORCE ABORT ALL ACTIVE DMA OPERATIONS ===
    // If a DMA channel is stuck waiting for a DREQ, forcing a hardware reset 
    // without aborting first can cause a permanent bus stall.
    for (int i = 0; i < NUM_DMA_CHANNELS; i++) {
        dma_channel_abort(i);
        // Clear any lingering interrupt requests
        dma_hw->ints0 = (1u << i);
        dma_hw->ints1 = (1u << i);
    }

    // === STAGE 2: PULL PERIPHERALS INTO HARDWARE RESET ===
    // This instantly clears all internal registers, state machines, and FIFOs.
    // We target DMAs, both PIO blocks, PWMs, I2C engines, and the HSTX block.
    reset_block(
        RESETS_RESET_DMA_BITS  | 
        RESETS_RESET_PIO0_BITS | 
        RESETS_RESET_PIO1_BITS | 
        RESETS_RESET_PWM_BITS  | 
        RESETS_RESET_I2C0_BITS | 
        RESETS_RESET_I2C1_BITS |
        RESETS_RESET_HSTX_BITS   // Exclusive to the RP2350 architecture
    );

    // === STAGE 3: RELEASE PERIPHERALS FROM RESET ===
    // Peripherals cannot be accessed until they are pulled out of reset 
    // and their internal clock distribution stabilizes.
    unreset_block_wait(
        RESETS_RESET_DMA_BITS  | 
        RESETS_RESET_PIO0_BITS | 
        RESETS_RESET_PIO1_BITS | 
        RESETS_RESET_PWM_BITS  | 
        RESETS_RESET_I2C0_BITS | 
        RESETS_RESET_I2C1_BITS |
        RESETS_RESET_HSTX_BITS
    );
}

void setup() {

  Serial.begin();
  delay(2000);
  Serial.println("START");

  //restart_all_rp2350_resources();//still starts on frame 8

  //I2C patch for mis-routed pin to imu on prototype
  pinMode(15, OUTPUT);
  digitalWrite(15, HIGH);

  // put your setup code here, to run once:
  scatterer_gatherer_engine.begin();
  light_sensor.begin(I2C0_SDA,I2C0_SCL,400'000);
  scatterer_gatherer_engine.registerSource(&light_sensor);

  frame_id=0;
  Serial.println("DONE setup");
}

void loop() {
  uint32_t brightness=light_sensor.getBrightness();
  Serial.println(brightness);

  // put your main code here, to run repeatedly:
  scatterer_gatherer_engine.compileAndRun(frame_id++,0,0);
  delay(500);
}
