/*
 * RP2350B I2C Sensor Capture Sketch
 * ---------------------------------
 * - GPIO 15: Output, set HIGH (Power/Enable pin)
 * - GPIO 12: I2C0 SDA
 * - GPIO 13: I2C0 SCL
 * - I2C Frequency: 400kHz
 * - Slaves: 
 *    - LSM6DS3TR (Accel/Gyro) @ 0x6B
 *    - LTR-308ALS-01 (Light) @ 0x53
 * - Function: Prints Accel (XYZ) and Brightness data every 1 second.
 */

#include <Wire.h>

// Pin Definitions
#define POWER_PIN 15
#define I2C0_SDA 12
#define I2C0_SCL 13

// I2C Addresses
#define LSM6DS3_ADDR 0x6B
#define LTR308_ADDR  0x53

void setup() {
  Serial.begin(115200);
  while (!Serial); // Wait for USB Serial
  
  // 1. Set GPIO 15 HIGH
  pinMode(POWER_PIN, OUTPUT);
  digitalWrite(POWER_PIN, HIGH);
  delay(100); // Give sensors time to power up

  // 2. Configure I2C0 (Wire) on GPIO 12/13 at 400kHz
  Wire.setSDA(I2C0_SDA);
  Wire.setSCL(I2C0_SCL);
  Wire.begin();
  Wire.setClock(400000);

  Serial.println("Initializing Sensors...");

  // 3. Initialize LSM6DS3 (Basic Power-Up)
  // Register 0x10 is CTRL1_XL (Accel). Set to 0x40 (104Hz, 2g range)
  Wire.beginTransmission(LSM6DS3_ADDR);
  Wire.write(0x10); 
  Wire.write(0x40);
  Wire.endTransmission();

  // 4. Initialize LTR-308ALS (Basic Power-Up)
  // Register 0x00 is MAIN_CTRL. Set bit 1 to 1 for Active Mode.
  Wire.beginTransmission(LTR308_ADDR);
  Wire.write(0x00);
  Wire.write(0x02);
  Wire.endTransmission();
}

void loop() {
  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate >= 1000) {
    lastUpdate = millis();

    readLSM6DS3();
    readLTR308();
    Serial.println("-------------------------");
  }
}

void readLSM6DS3() {
  // Accel data starts at 0x28 (XL, XH, YL, YH, ZL, ZH)
  Wire.beginTransmission(LSM6DS3_ADDR);
  Wire.write(0x28);
  if (Wire.endTransmission() != 0) {
    Serial.println("LSM6DS3 not found!");
    return;
  }

  Wire.requestFrom(LSM6DS3_ADDR, 6);
  if (Wire.available() == 6) {
    int16_t x = Wire.read() | (Wire.read() << 8);
    int16_t y = Wire.read() | (Wire.read() << 8);
    int16_t z = Wire.read() | (Wire.read() << 8);

    // Convert raw to Gs (assuming +/- 2g range, 16-bit)
    float scaling = 0.061 / 1000.0; 
    Serial.print("Accel Gs: X="); Serial.print(x * scaling);
    Serial.print(" Y="); Serial.print(y * scaling);
    Serial.print(" Z="); Serial.println(z * scaling);
  }
}

void readLTR308() {
  // LTR-308 Data is in DATA_0, DATA_1, DATA_2 (0x0D, 0x0E, 0x0F)
  Wire.beginTransmission(LTR308_ADDR);
  Wire.write(0x0D);
  if (Wire.endTransmission() != 0) {
    Serial.println("LTR-308 not found!");
    return;
  }

  Wire.requestFrom(LTR308_ADDR, 3);
  if (Wire.available() == 3) {
    uint32_t d0 = Wire.read();
    uint32_t d1 = Wire.read();
    uint32_t d2 = Wire.read();
    
    // Combine 20-bit data
    uint32_t lux_raw = d0 | (d1 << 8) | ((d2 & 0x0F) << 16);
    Serial.print("Brightness (Raw 20-bit): ");
    Serial.println(lux_raw);
  }
}
