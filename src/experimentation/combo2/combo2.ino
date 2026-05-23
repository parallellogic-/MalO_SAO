#include "dma_control_block.h"
#include "light_sensor.h"
#include "imu.h"

ScatterGatherEngine scatterer_gatherer_engine;
LightSensor light_sensor(i2c0);
IMU imu(i2c0);

#define I2C0_SDA 12
#define I2C0_SCL 13

uint32_t frame_id=0;

void setup() {

  //I2C patch for mis-routed pin to imu on prototype
  pinMode(15, OUTPUT);
  //digitalWrite(15, LOW);
  //delay(100); 
  digitalWrite(15, HIGH);
  delay(20);  //AN4650 needed for reboot time of IMU

  Serial.begin();
  long start_tms=millis();
  while(!Serial && (millis()-start_tms)<6000);//wait for terminal to connect or timeout, whichever is first
  Serial.println("START");

  //restart_all_rp2350_resources();//still starts on frame 8

  //init shared i2c bus
  i2c_init(i2c0, 400'000); //PRECON: imu reboot delay assumes 400 kHz.  if lower speed, need to increase reboot time
  gpio_set_function(I2C0_SDA, GPIO_FUNC_I2C);
  gpio_set_function(I2C0_SCL, GPIO_FUNC_I2C);
  //gpio_pull_up(I2C0_SDA);
  //gpio_pull_up(I2C0_SCL);

  // put your setup code here, to run once:
  scatterer_gatherer_engine.begin();
  light_sensor.begin();
  imu.begin();
  scatterer_gatherer_engine.registerSource(&light_sensor);
  scatterer_gatherer_engine.registerSource(&imu);
  

  frame_id=0;
  Serial.println("DONE setup");

  //pinMode(15, OUTPUT);
  //digitalWrite(15, LOW);
  //delay(100); 
  //digitalWrite(15, HIGH);
  //delay(5000); 
  //Serial.begin(115200);
  //while (!Serial); // Wait for terminal connection
  //Serial.println("START");
  //while(1) loop4();
}

void loop() {
  uint32_t brightness=light_sensor.getBrightness();
  Serial.print(frame_id);
  Serial.print(" frame, ");
  Serial.print(brightness);
  Serial.print(" light, ");

  uint8_t fifo_count=imu.get_fifo_sample_count();
  Serial.print(fifo_count,HEX);
  Serial.print(" imu_reg, ");
  Serial.print(fifo_count-6*(fifo_count/6));
  Serial.println(" imu_reg2, ");

  // setup and run next batch
  scatterer_gatherer_engine.compileAndRun(frame_id++,0,0);

  uint32_t start_tms=millis();
  bool is_first=true;
  bool last_status=false;
  while(millis()<(start_tms+16))
  {
    

    // Read the full IC_STATUS register
    uint32_t statusReg = i2c0->hw->status;

    // The is_busy flag is bit 12 of the IC_STATUS register
    bool isBusy = (statusReg & I2C_IC_STATUS_ACTIVITY_BITS) != 0; 

    if(is_first or isBusy!=last_status)
    {
      // Print in hexadecimal format
      Serial.print("Full IC_STATUS Register: 0x");
      Serial.println(statusReg, HEX);

      
      Serial.print("I2C is_busy status: ");
      Serial.println(isBusy ? "BUSY" : "IDLE");
      is_first=false;
      last_status=isBusy;
    }
  }

  //delay(16);
  //delay(1000);
}

/*#include "hardware/timer.h"
#include "pico/stdlib.h"

// Define the precise 120 Hz period in microseconds (1,000,000 / 120)
const uint32_t TIMER_INTERVAL_US = 8333; 

// Track the Repeating Timer structure
struct repeating_timer timer;

// CRITICAL: Any variables modified inside an IRQ must be marked volatile
volatile bool trigger_dma_flag = false;

// 1. The 120 Hz Hardware IRQ Callback Function
bool timer_callback_120hz(struct repeating_timer *t) {
    // Keep this function incredibly fast. No Serial.print() or delay() allowed here.
    
    // Example: Signal your main loop or DMA controller to perform an action
    trigger_dma_flag = true;
    
    return true; // Return true to keep the repeating timer running continuously
}

void setup() {
    Serial.begin(115200);
    
    // Initialize the built-in LED for visual verification
    pinMode(LED_BUILTIN, OUTPUT);

    // 2. Hardware Timer Initialization
    // Arguments: (interval_in_us, callback_name, user_data_pointer, timer_struct_pointer)
    // A negative interval ensures accurate spacing from the START of each callback execution.
    bool success = add_repeating_timer_us(-TIMER_INTERVAL_US, timer_callback_120hz, NULL, &timer);
    
    if (!success) {
        Serial.println("Failed to initialize 120 Hz Hardware Timer!");
        while (1); // Halt if timer creation fails
    }
}

void loop() {
    // 3. Process the high-priority IRQ event safely in the main loop thread
    if (trigger_dma_flag) {
        trigger_dma_flag = false; // Reset the flag immediately
        
        // Toggle the LED every 1/120th of a second
        digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    }
}*/
