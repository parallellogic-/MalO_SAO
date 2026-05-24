#include <Wire.h>
#include <SparkFunLSM6DS3.h>

// Create an instance of the LSM6DS3 class
LSM6DS3 myIMU(I2C_MODE, 0x6B);

#define POWER_PIN 15
#define I2C0_SDA 12
#define I2C0_SCL 13

void setup() {
  pinMode(POWER_PIN, OUTPUT);
  digitalWrite(POWER_PIN, LOW);
  delay(100); 
  digitalWrite(POWER_PIN, HIGH);
  delay(100); 
  Serial.begin(115200);
  while (!Serial); // Wait for terminal connection
  Serial.println("START");

  // Initialize I2C and the IMU
  //myIMU.begin();
  Wire.setSDA(I2C0_SDA);
  Wire.setSCL(I2C0_SCL);
  Wire.begin();
  Wire.setClock(100000);
  Serial.println("START2");
  /*if (myIMU.begin() != 0) {
    Serial.println("Device error. Check wiring!");
    while (1);
  }*/

  // --- IMU Configuration for FIFO ---
  // 1. Enable Accelerometer and Gyro
  myIMU.settings.accelEnabled = 1;
  myIMU.settings.gyroEnabled = 1;

  // 2. Set high-performance / ODR modes
  myIMU.settings.accelSampleRate = 104; // 104 Hz
  myIMU.settings.gyroSampleRate = 104;  // 104 Hz
  
  // 3. Configure FIFO Settings
  //myIMU.settings.fifoFifoEnabled = 1;
  myIMU.settings.gyroFifoEnabled = 1;
  myIMU.settings.accelFifoEnabled = 1;
  myIMU.settings.fifoModeWord = 6; // Set to Continuous mode
  
  // Apply settings to the chip
  Serial.println("START4");
  //myIMU.beginCore();
  Serial.println("START3");


  // 4. MANUAL REGISTER INJECTION 
  // Since beginCore doesn't touch settings, we write directly to the registers:
  
  // Set Accel: 104 Hz (0x05), Scale +/-16g (0x01), LPF2 enabled (0x01) -> 0x55
  myIMU.writeRegister(0x08, 0x09); 
  myIMU.writeRegister(0x10, 0x55); 
  
  // Set Gyro: 104 Hz (0x05), Scale 2000 dps (0x03) -> 0x5C
  myIMU.writeRegister(0x11, 0x5C); 
  
  // Configure FIFO Decimation: Gyro decimation = 1 (0x01<<3), Accel decimation = 1 (0x01) -> 0x09
  
  
  // Set FIFO ODR to 104Hz and mode to Continuous Mode
  // ODR Bits [6:3] = 0101 (104 Hz) -> 0x28
  // Mode Bits [2:0] = 110 (Continuous Mode) -> 0x06 -> Combined: 0x2E
  //myIMU.writeRegister(0x0A, 0x2E);
  myIMU.writeRegister(0x0A, (0x01 << 3) | 0x06);
  Serial.println("Registers pushed successfully! FIFO Engine started.");
}

void loop() {
  //Serial.println("loop");
  uint8_t unreadWords = 0;
  uint8_t unreadBytes = 0;

  // The library expects: readRegister(&destinationVariable, registerAddress)
  // Hardcoded hex values match the internal LSM6DS3 register map
  myIMU.readRegister(&unreadWords, 0x3A); // FIFO_STATUS1
  //myIMU.readRegister(&unreadBytes, 0x3B); // FIFO_STATUS2
  
  // Assemble 11-bit FIFO unread sample count
  uint16_t fifoSamples = ((unreadBytes & 0x0F) << 8) | unreadWords;
  
  // Each complete 6-DoF sample takes 6 words (Ax, Ay, Az, Gx, Gy, Gz)
  uint16_t completeSets = fifoSamples / 6; 

  if (completeSets > 0) {
    Serial.print("Streaming ");
    Serial.print(completeSets);
    Serial.println(" sample sets:");

    for (int i = 0; i < completeSets; i++) {
      // Read 6 consecutive 16-bit values from FIFO
      // Each fifoRead() call pulls a single 16-bit axis value
      int16_t gx = myIMU.fifoRead();
      int16_t gy = myIMU.fifoRead();
      int16_t gz = myIMU.fifoRead();
      int16_t ax = myIMU.fifoRead();
      int16_t ay = myIMU.fifoRead();
      int16_t az = myIMU.fifoRead();

      // Print raw output 
      Serial.print("A: ");
      Serial.print(ax); Serial.print(", ");
      Serial.print(ay); Serial.print(", ");
      Serial.print(az); Serial.print(" | ");
      
      Serial.print("G: ");
      Serial.print(gx); Serial.print(", ");
      Serial.print(gy); Serial.print(", ");
      Serial.println(gz);
    }
  }

  // Poll periodically
  delay(16); 
}