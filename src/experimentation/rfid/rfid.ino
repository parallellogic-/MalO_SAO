//i have rp2350b, arduino ide
//it has ST25DV04K-IER6C3 connected on I2C gp12 (CLK) and gp13 (DATA)
//show a demo that enables energy harvesting and programs a URL (even when the chips loses power): parallellogic-.github.io/MalO_SAO

#include <hardware/i2c.h>
#include <hardware/gpio.h>

// ST25DV04K I2C Device Addresses
#define ST25DV_ADDR_USER   0x53  
#define ST25DV_ADDR_SYSTEM 0x57  

// Native RP2350 Hardware Allocation
#define I2C_PORT    i2c0       
#define PIN_SCL     12         // GP12
#define PIN_SDA     13         // GP13
#define I2C_SPEED   400000     // 400 kHz Fast Mode

const int pin=38;

// Native SDK Wrapper for System Register Writes
bool writeSystemRegister_Native(uint16_t reg_addr, uint8_t value) {
  uint8_t pkt[3];
  pkt[0] = (uint8_t)(reg_addr >> 8);   // MSB
  pkt[1] = (uint8_t)(reg_addr & 0xFF); // LSB
  pkt[2] = value;
  
  int result = i2c_write_blocking(I2C_PORT, ST25DV_ADDR_SYSTEM, pkt, 3, false);
  return (result == 3);
}

// FIXED: Corrected buffer sizing and indexing to prevent stack corruption
bool writeUserMemory_Native(uint16_t mem_addr, uint8_t *data, uint8_t len) {
  if (len > 34) return false; // Safety cap for structural segments
  
  uint8_t pkt[36]; // Room for 2-byte address + payload
  pkt[0] = (uint8_t)(mem_addr >> 8);
  pkt[1] = (uint8_t)(mem_addr & 0xFF);
  memcpy(&pkt[2], data, len);
  
  int result = i2c_write_blocking(I2C_PORT, ST25DV_ADDR_USER, pkt, len + 2, false);
  return (result == (len + 2));
}

void setup() {
  Serial.begin(115200);
  while (!Serial && millis() < 7000); // Wait for Serial monitor
  pinMode(pin, OUTPUT);
  Serial.begin();
  
  Serial.println("\n--- RP2350 Native SDK ST25DV Demo ---");

  // ==========================================
  // NATIVE HARDWARE INITIALIZATION
  // ==========================================
  Serial.println("Initializing RP2350 Hardware Drivers...");
  i2c_init(I2C_PORT, I2C_SPEED);
  
  gpio_set_function(PIN_SDA, GPIO_FUNC_I2C);
  gpio_set_function(PIN_SCL, GPIO_FUNC_I2C);
  gpio_pull_up(PIN_SDA);
  gpio_pull_up(PIN_SCL);

  // ==========================================
  // STEP 1: PRESENT I2C PASSWORD
  // ==========================================
  Serial.println("Presenting I2C password via hardware block...");
  uint8_t pwd_pkt[11];
  pwd_pkt[0] = 0x09; // MSB Password Validation Reg 
  pwd_pkt[1] = 0x00; // LSB Password Validation Reg
  pwd_pkt[2] = 0x09; // Validation command token
  memset(&pwd_pkt[3], 0x00, 8); // 8 bytes of zero password
  
  int pwd_status = i2c_write_blocking(I2C_PORT, ST25DV_ADDR_SYSTEM, pwd_pkt, 11, false);
  if (pwd_status != 11) {
    Serial.printf("Error: Security presentation failed with SDK status: %d\n", pwd_status);
    while (1);
  }
  delay(10); 

  // ==========================================
  // STEP 2: ENABLE NON-VOLATILE ENERGY HARVESTING
  // ==========================================
  Serial.println("Programming static Energy Harvesting Mode...");
  if (writeSystemRegister_Native(0x0002, 0x00)) { 
    Serial.println("Success: Energy Harvesting forced ON after boot!");
  } else {
    Serial.println("Error programming static register EH_MODE.");
  }
  delay(10); 

  // ==========================================
  // STEP 3: PROGRAM PERSISTENT NDEF URL
  // ==========================================
  Serial.println("Writing NDEF Structure...");

  // Capability Container File (CC File)
  uint8_t cc_file[] = {0xE1, 0x40, 0x40, 0x00};
  if (writeUserMemory_Native(0x0000, cc_file, 4)) {
    Serial.println("-> CC File written successfully.");
  } else {
    Serial.println("-> Error writing CC File.");
  }
  delay(10); 

  // URL payload string
  char url[] = "parallellogic-.github.io/MalO_SAO";
  uint8_t url_len = sizeof(url) - 1; 

  // NDEF structural markers
  uint8_t ndef_header[] = {
    0x03,                           // NDEF TLV Tag
    (uint8_t)(url_len + 5),         // Total structure length
    0xD1, 0x01,                     // Record Headers
    (uint8_t)(url_len + 1),         // Length of dynamic payload 
    0x55,                           // URI Record Type
    0x03                            // "http://" prefix code
  };

  // Push Header Block
  if (writeUserMemory_Native(0x0004, ndef_header, 7)) {
    Serial.println("-> NDEF Header written successfully.");
  } else {
    Serial.println("-> Error writing NDEF Header.");
  }
  delay(10);

  // Push URL String Data
  if (writeUserMemory_Native(0x000B, (uint8_t*)url, url_len)) {
    Serial.println("-> URL payload written successfully.");
  } else {
    Serial.println("-> Error writing URL payload.");
  }
  delay(10);

  // Push Structural Terminator Byte
  uint8_t terminator = 0xFE;
  if (writeUserMemory_Native(0x000B + url_len, &terminator, 1)) {
    Serial.println("-> Terminator byte written successfully.");
  } else {
    Serial.println("-> Error writing Terminator byte.");
  }
  delay(10);

  Serial.println("✅ Flashing Complete! You may safely disconnect power.");
}

void loop() {
  digitalWrite(pin, HIGH);  // change state of the LED by setting the pin to the HIGH voltage level
  delay(1000);                      // wait for a second
  digitalWrite(pin, LOW);   // change state of the LED by setting the pin to the LOW voltage level
  delay(1000);                      // wait for a second
  Serial.println(pin,DEC);
}