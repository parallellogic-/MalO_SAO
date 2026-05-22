#include "dma_control_block.h"
#include "light_sensor.h"
#include "imu.h"

ScatterGatherEngine scatterer_gatherer_engine;
LightSensor light_sensor(i2c0);
IMU imu(i2c0);

#define I2C0_SDA 12
#define I2C0_SCL 13

uint32_t frame_id=0;

#include <Wire.h>
void setup3() {
  pinMode(15, OUTPUT);
  digitalWrite(15, HIGH);
  // Initialize USB Serial communication for debugging
  Serial.begin(115200);
  while (!Serial) {
    delay(10); // Wait for the terminal window to open
  }
  
  Serial.println("Initializing I2C0 at 400kHz...");

  // 1. Assign the custom SDA and SCL pins to the Wire (i2c0) instance
  Wire.setSDA(I2C0_SDA);
  Wire.setSCL(I2C0_SCL);

  // 2. Start I2C bus as a Master
  Wire.begin();

  // 3. Force the I2C clock speed to 400 kHz (Fast Mode)
  Wire.setClock(400000);

  Serial.println("I2C Setup complete. Probing LSM6DS3...");
}

void loop3() {
  // 1. Begin a write transaction to tell the sensor which register we want to read
  Wire.beginTransmission(LSM6DS_ADDR);
  Wire.write(REG_WHO_AM_I);
  
  // End transmission but keep the connection active (send a REPEATED START)
  byte error = Wire.endTransmission(false); 

  if (error == 0) {
    // 2. Request 1 byte back from the LSM6DS3
    Wire.requestFrom(LSM6DS_ADDR, 1);
    
    if (Wire.available()) {
      byte whoAmIValue = Wire.read();
      
      // Print the output in Hexadecimal format
      Serial.print("LSM6DS3 WHO_AM_I Register Value: 0x");
      if (whoAmIValue < 16) Serial.print("0"); // Pad with leading zero
      Serial.println(whoAmIValue, HEX);
      
      // Expected value for standard LSM6DS3 is typically 0x69 or 0x6C (for TR-C version)
    } else {
      Serial.println("Error: No data received from register.");
    }
  } else {
    Serial.print("Error communicating with device. I2C Error code: ");
    Serial.println(error);
    Serial.println("Check your wiring and pull-up resistors.");
  }

  // Poll every 2 seconds
  delay(2000);
}

void setup() {

  Serial.begin();
  delay(2000);
  Serial.println("START");

  //restart_all_rp2350_resources();//still starts on frame 8

  //I2C patch for mis-routed pin to imu on prototype
  pinMode(15, OUTPUT);
  digitalWrite(15, HIGH);

  //init shared i2c bus
  i2c_init(i2c0, 400'000);
  gpio_set_function(I2C0_SDA, GPIO_FUNC_I2C);
  gpio_set_function(I2C0_SCL, GPIO_FUNC_I2C);
  //gpio_pull_up(I2C0_SDA);
  //gpio_pull_up(I2C0_SCL);

  // put your setup code here, to run once:
  scatterer_gatherer_engine.begin();
  light_sensor.begin();
  imu.begin();
  scatterer_gatherer_engine.registerSource(&imu);
  scatterer_gatherer_engine.registerSource(&light_sensor);

  frame_id=0;
  Serial.println("DONE setup");
}

void loop() {
  uint32_t brightness=light_sensor.getBrightness();
  Serial.print(brightness);
  Serial.print(" light, ");

  uint8_t fifo_count=imu.get_fifo_sample_count();
  Serial.println(fifo_count);

  // setup and run next batch
  scatterer_gatherer_engine.compileAndRun(frame_id++,0,0);
  delay(16);
  //delay(1000);
}
