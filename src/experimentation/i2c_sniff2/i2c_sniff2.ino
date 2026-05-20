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
 * - Function: Prints Accel (XYZ), Gyro (XYZ), and Brightness every 0.3 second.
 */

#include <Wire.h>

#define POWER_PIN 15
#define I2C0_SDA 12
#define I2C0_SCL 13
#define LSM6DS3_ADDR 0x6B
#define LTR308_ADDR  0x53

void setup() {
  Serial.begin(115200);
  while (!Serial);
  
  pinMode(POWER_PIN, OUTPUT);
  digitalWrite(POWER_PIN, HIGH);
  delay(100); 

  Wire.setSDA(I2C0_SDA);
  Wire.setSCL(I2C0_SCL);
  Wire.begin();
  Wire.setClock(400000);
/*
  // Initialize LSM6DS3
  // CTRL1_XL (0x10): Accel = 104Hz, 2g range -> 0x40
  Wire.beginTransmission(LSM6DS3_ADDR);
  Wire.write(0x10); 
  Wire.write(0x40);
  Wire.endTransmission();

  // CTRL2_G (0x11): Gyro = 104Hz, 250 dps range -> 0x40
  Wire.beginTransmission(LSM6DS3_ADDR);
  Wire.write(0x11); 
  Wire.write(0x40);
  Wire.endTransmission();*/

  // Initialize LTR-308ALS
  Wire.beginTransmission(LTR308_ADDR);
  Wire.write(0x00);
  Wire.write(0x02);
  Wire.endTransmission();
}

void loop() {
  static unsigned long lastUpdate = 0;
  if (millis() - lastUpdate >= 300) {
    lastUpdate = millis();
    //readLSM6DS3();
    readLTR308();
    Serial.println("-------------------------");
  }
}

void readLSM6DS3() {
  // Read Gyro (0x22-0x27) and Accel (0x28-0x2D) in one 12-byte burst
  Wire.beginTransmission(LSM6DS3_ADDR);
  Wire.write(0x22); 
  if (Wire.endTransmission() != 0) {
    Serial.println("LSM6DS3 not found!");
    return;
  }

  Wire.requestFrom(LSM6DS3_ADDR, 12);
  if (Wire.available() == 12) {
    // Gyro Raw
    int16_t gx = Wire.read() | (Wire.read() << 8);
    int16_t gy = Wire.read() | (Wire.read() << 8);
    int16_t gz = Wire.read() | (Wire.read() << 8);
    // Accel Raw
    int16_t ax = Wire.read() | (Wire.read() << 8);
    int16_t ay = Wire.read() | (Wire.read() << 8);
    int16_t az = Wire.read() | (Wire.read() << 8);

    // Scaling for 2g range: 0.061 mg/LSB
    float a_scale = 0.000061; 
    // Scaling for 250 dps range: 8.75 mdps/LSB
    float g_scale = 0.00875;

    Serial.print("Accel [G]: X="); Serial.print(ax * a_scale);
    Serial.print(" Y="); Serial.print(ay * a_scale);
    Serial.print(" Z="); Serial.println(az * a_scale);

    Serial.print("Gyro [dps]: X="); Serial.print(gx * g_scale);
    Serial.print(" Y="); Serial.print(gy * g_scale);
    Serial.print(" Z="); Serial.println(gz * g_scale);
  }
}

void readLTR308() {
  Wire.beginTransmission(LTR308_ADDR);
  Wire.write(0x0D);
  if (Wire.endTransmission() == 0) {
    Wire.requestFrom(LTR308_ADDR, 3);
    if (Wire.available() == 3) {
      uint32_t d0 = Wire.read();
      uint32_t d1 = Wire.read();
      uint32_t d2 = Wire.read();
      Serial.print(d0,HEX); Serial.print(" "); Serial.print(d1,HEX); Serial.print(" "); Serial.println(d2,HEX);
      uint32_t lux_raw = d0 | (d1 << 8) | ((d2 & 0x0F) << 16);
      Serial.print("Brightness (Raw): "); Serial.println(lux_raw);
    }
  }
}
