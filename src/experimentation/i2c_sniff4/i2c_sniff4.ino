#include <stdio.h>
#include "pico/stdlib.h"
#include "hardware/i2c.h"
#include "hardware/gpio.h"

// Hardware Definitions
#define I2C_PORT      i2c0
#define DEV_ADDR      0x6B
#define POWER_PIN     15
#define I2C0_SDA      12
#define I2C0_SCL      13
#define WHO_AM_I_REG      0x0F
#define I2C_BAUDRATE  400000 // 400 kHz

// Native function to write to an I2C register
void writeRegister(uint8_t reg, uint8_t value) {
    uint8_t buf[2] = {reg, value};
    // Send 2 bytes: register address, then data value
    i2c_write_blocking(I2C_PORT, DEV_ADDR, buf, 2, false);
}

// Native function to read from an I2C register
void readRegister(uint8_t *dest, uint8_t reg) {
    // Phase 1: Write the register pointer address (nostop = true to maintain bus control)
    i2c_write_blocking(I2C_PORT, DEV_ADDR, &reg, 1, true);
    // Phase 2: Read the value back from the slave device
    i2c_read_blocking(I2C_PORT, DEV_ADDR, dest, 1, false);
}

void setup() {
  // 1. Initialize Power GPIO pin
  gpio_init(POWER_PIN);
  gpio_set_dir(POWER_PIN, GPIO_OUT);
  
  gpio_put(POWER_PIN, 0);
  sleep_ms(100); 
  gpio_put(POWER_PIN, 1);
  sleep_ms(100); 

  sleep_ms(2000); // Give time for USB serial monitor to attach
  Serial.begin();
  Serial.print("START");

  // 2. Initialize Native RP2350 I2C0 Hardware
  i2c_init(I2C_PORT, I2C_BAUDRATE);
  
  // 3. Map GPIOs to the I2C Peripheral function
  gpio_set_function(I2C0_SDA, GPIO_FUNC_I2C);
  gpio_set_function(I2C0_SCL, GPIO_FUNC_I2C);


  // 4. MANUAL REGISTER INJECTION (Via Native I2C calls)
  // Set Accel: 104 Hz (0x05), Scale +/-16g (0x01), LPF2 enabled (0x01) -> 0x55
  writeRegister(0x08, 0x09); 
  writeRegister(0x10, 0x55); 
  
  // Set Gyro: 104 Hz (0x05), Scale 2000 dps (0x03) -> 0x5C
  writeRegister(0x11, 0x5C); 
  
  // Set FIFO ODR to 104Hz and mode to Continuous Mode
  // ODR Bits [6:3] = 0101 (104 Hz) -> 0x28
  // Mode Bits [2:0] = 110 (Continuous Mode) -> 0x06 -> Combined: 0x2E
  writeRegister(0x0A, 0x2E);

  Serial.print("Registers pushed successfully! FIFO Engine started.");
}

void loop() {
  Serial.printf("loop\n");
  uint8_t unreadWords = 0;
  uint8_t unreadBytes = 0;

  // Read data via custom native register read functions
  readRegister(&unreadWords, 0x3A); // FIFO_STATUS1
  // readRegister(&unreadBytes, 0x3B); // FIFO_STATUS2 (uncomment if needed)
  
  // Assemble 11-bit FIFO unread sample count
  uint16_t fifoSamples = ((unreadBytes & 0x07) << 8) | unreadWords;
  
  // Each complete 6-DoF sample takes 6 words (Ax, Ay, Az, Gx, Gy, Gz)
  uint16_t completeSets = fifoSamples / 6; 

  if (completeSets > 0) {
    Serial.printf("Streaming 0x%02X samples:\n", fifoSamples);
    
    // Optional FIFO data parsing goes here
  }

  // Poll periodically
  sleep_ms(16); 
}
