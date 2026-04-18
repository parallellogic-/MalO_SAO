#include "hardware/i2c.h"
#include "pico/stdlib.h"

//#define I2C_PORT i2c1
//#define SDA_PIN 46
//#define SCL_PIN 47
#define I2C_PORT i2c0
#define SDA_PIN 12
#define SCL_PIN 13
#define ADDR 0x53 //A6 / A7
#define REG 0x06

void setup() {
  Serial.begin(115200);
  while (!Serial); // Wait for Serial to be ready

  pinMode(15, OUTPUT);
  digitalWrite(15, HIGH);  // give power to IMU - needed for r1 because of swap with IMU_INT2 and VDD

  // 1. Initialise I2C at 400kHz
  i2c_init(I2C_PORT, 400 * 1000);
  
  // 2. Set GPIO functions to I2C
  gpio_set_function(SDA_PIN, GPIO_FUNC_I2C);
  gpio_set_function(SCL_PIN, GPIO_FUNC_I2C);
  
  // 3. Enable pull-ups (mandatory if hardware ones are missing)
  //gpio_pull_up(SDA_PIN);
  //gpio_pull_up(SCL_PIN);

  Serial.println("--- Boot: I2C Init Complete ---");
}

void loop() {
  uint8_t reg_addr = REG;
  uint8_t rx_data = 0;

  // Step 1: Write the register address
  // 'true' keeps the master in control of the bus (Repeated Start)
  int write_res = i2c_write_timeout_per_char_us(I2C_PORT, ADDR, &reg_addr, 1, true, 10000);

  if (write_res < 0) {
    Serial.println("Write Failed: Device not responding (Timeout/NACK)");
  } else {
    // Step 2: Read the value
    int read_res = i2c_read_timeout_per_char_us(I2C_PORT, ADDR, &rx_data, 1, false, 10000);
    
    if (read_res < 0) {
      Serial.println("Read Failed: Timeout");
    } else {
      Serial.print("Success! Value at 0x06: 0x");
      Serial.println(rx_data, HEX);
    }
  }

  delay(200);
}
