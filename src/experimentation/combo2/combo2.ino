#include "dma_control_block.h"
#include "light_sensor.h"
#include "imu.h"
#include "screen.h"
#include "led.h"
#include <hardware/watchdog.h>
#include "hardware/pio.h"
#include "hardware/clocks.h"
#include "hardware/pwm.h"
#include "hardware/resets.h"

#include <Wire.h>

ScatterGatherEngine scatterer_gatherer_engine;
LightSensor light_sensor(i2c0);
IMU imu(i2c0);
Screen screen(spi1);
Charlieplex led_lower(0);
Charlieplex led_upper(1);

#define I2C0_SDA 12
#define I2C0_SCL 13
#define I2C0_BAUD 400'000

#define SPI1_CS 9
#define SPI1_DC 8
#define SPI1_MOSI 11
#define SPI1_SCLK 10
#define SPI1_BAUD 8'000'000

uint32_t frame_id=0;

void setup() {

  //I2C patch for mis-routed pin to imu on prototype
  pinMode(15, OUTPUT);
  digitalWrite(15, HIGH);
  delay(20);  //AN4650 needed for reboot time of IMU --> later, put as part of boot-up sequence routine

  Serial.begin();
  long start_tms=millis();
  while(!Serial && (millis()-start_tms)<7000);//wait for terminal to connect or timeout, whichever is first
  Serial.println("START");

  Serial.println("Init I2C...");
  if(0)
  {
    //init shared i2c bus
    i2c_init(i2c0, I2C0_BAUD); //PRECON: imu reboot delay assumes 400 kHz.  if lower speed, need to increase reboot time
    gpio_set_function(I2C0_SDA, GPIO_FUNC_I2C);
    gpio_set_function(I2C0_SCL, GPIO_FUNC_I2C);
  }else{
    Wire.setSDA(I2C0_SDA);
    Wire.setSCL(I2C0_SCL);
    Wire.begin();
    Wire.setClock(I2C0_BAUD);
  }

  Serial.println("Init SPI...");
  spi_init(spi1, SPI1_BAUD);
  gpio_set_function(SPI1_SCLK, GPIO_FUNC_SPI);
  gpio_set_function(SPI1_MOSI, GPIO_FUNC_SPI);
  
  Serial.println("Init LEDs...");
  led_upper.begin();
  led_lower.begin();

  // Set CS and DC pins as GPIO outputs (software controlled)
  gpio_init(SPI1_CS);
  gpio_set_dir(SPI1_CS, GPIO_OUT);
  gpio_put(SPI1_CS, HIGH); // CS high by default (inactive)

  gpio_init(SPI1_DC);
  gpio_set_dir(SPI1_DC, GPIO_OUT);

  //optional force-clear i2c on every use
  //i2c_deinit(i2c0);
  //i2c_init(i2c0, 400000);
  scatterer_gatherer_engine.begin(true);
  //placeholder_begin();
  screen.begin();
  light_sensor.begin();
  imu.begin();
  scatterer_gatherer_engine.registerSource(&screen);
  scatterer_gatherer_engine.registerSource(&light_sensor);
  scatterer_gatherer_engine.registerSource(&imu);
  scatterer_gatherer_engine.registerSource(&scatterer_gatherer_engine);//register self to perform end-of-cycle completion check
  
  Serial.println("DONE setup");
}

void loop() {
  uint32_t brightness=light_sensor.getBrightness();
  Serial.printf("frame_id: %d, brightness: %d, ",frame_id,brightness);

  float imu_celsius=imu.get_celsius();
  uint32_t fifo_count=imu.get_fifo_sample_count();
  uint32_t sniff_ctrl=dma_hw->sniff_ctrl;
  uint32_t sniff_data=dma_hw->sniff_data;
  Serial.printf("imu_celsius: %.2f, fifo_count: %d, ",imu_celsius,fifo_count);

  Serial.printf("accel: %0.2f, %0.2f, %0.2f, gyro: %0.2f, %0.2f, %0.2f, ",imu.get_accel(0),imu.get_accel(1),imu.get_accel(2),imu.get_gyro(0),imu.get_gyro(1),imu.get_gyro(2));

  Serial.println();
  
  //placeholder_spi();

  // setup and run next batch
  uint32_t start_tms=millis();
  uint64_t start_time = time_us_64();
  bool is_imu_print_runtime=false;
  scatterer_gatherer_engine.compileAndRun(frame_id,0,0);


    

  bool is_first=true;
  bool last_status=false;
  while(millis()<(start_tms+16))
  {//core1 contents
    imu.update();
    

    /*if(!is_imu_print_runtime && imu._is_data_ready)
    {
      is_imu_print_runtime=true;
      // 2. Capture the finishing timestamp in microseconds
      uint64_t finish_time = time_us_64();

      // 3. Calculate total elapsed microseconds
      uint64_t elapsed_time = finish_time - start_time;

      Serial.printf("IMU Elapsed time: %llu microseconds\n", elapsed_time);
    }*/

    // Read the full IC_STATUS register
    uint32_t statusReg = i2c0->hw->status;

    // The is_busy flag is bit 12 of the IC_STATUS register
    bool isBusy = (statusReg & I2C_IC_STATUS_ACTIVITY_BITS) != 0; 

    //if(is_first or isBusy!=last_status)
    if(scatterer_gatherer_engine.is_dma_success(frame_id))
    {
      led_update();
    }
  }
  bool is_dma_success=scatterer_gatherer_engine.is_dma_success(frame_id);
  if(!is_dma_success)
  {
    Serial.println("DMA FAULT");
    while(1);
  }

  frame_id++;
}

float prev_accel=0.0;
void led_update()
{
// -- led update --
    led_upper.set_max_effective_led_count(CHARLIPLEX_LED_COUNT/2);
    led_lower.set_max_effective_led_count(14);

    for(uint8_t iter=0;iter<CHARLIPLEX_LED_COUNT;iter++)
    {
      //slow fade
      uint16_t brightness_upper = (-millis()/8)+iter*32; 
      if(brightness_upper & 0x0100) brightness_upper=255-(uint8_t)brightness_upper;//fade fully off half the time
      led_upper.set_brightness(iter,(uint8_t)brightness_upper);

      if(0)
      {
        //steeple chase, with background toggling ON/ODD
        int8_t brightness_lower = millis()/16000%2?0:32*2; 
        brightness_lower=iter<24?brightness_lower:brightness_lower/2;
        if((((iter%24)%5)==(millis()/(64*4))%5)) brightness_lower=iter<24?255:128;
        led_lower.set_brightness(iter,brightness_lower);
      }else{
        //float accel=imu.get_accel(1);
        /*int8_t led_0=(int8_t)accel;
        int8_t led_1=(int8_t)(accel+1);
        uint8_t brightness_1=(uint8_t)((accel-led_0)*255);
        uint8_t brightness_0=255-brightness_1;
        led_0=max(led_0,0);
        led_1=max(led_1,0);
        led_0=min(led_0,CHARLIPLEX_LED_COUNT/2-1);
        led_1=min(led_1,CHARLIPLEX_LED_COUNT/2-1);
        led_lower.set_brightness(led_0,brightness_0);
        led_lower.set_brightness(led_1,led_0!=led_1?brightness_1:max(brightness_0,brightness_1));
        led_lower.set_brightness(led_0+CHARLIPLEX_LED_COUNT/2,brightness_0);
        led_lower.set_brightness(led_1+CHARLIPLEX_LED_COUNT/2,led_0!=led_1?brightness_1:max(brightness_0,brightness_1));*/
        /*
        float accel=imu.get_gyro(2)/100.0;
        accel=(0.25-accel/4)*CHARLIPLEX_LED_COUNT;
        if(iter<CHARLIPLEX_LED_COUNT/2)
        {
          for(int is_green=0;is_green<2;is_green++)
          {
            float brightness=255*(1.4-abs(accel-iter+(is_green?-1.5:1.5))/3.0);
            brightness=max(brightness,0);
            brightness=min(brightness,255);
            uint8_t brightness8=(uint8_t)brightness;
            //led_lower.set_brightness(iter,brightness8);
            led_lower.set_brightness(iter+is_green*CHARLIPLEX_LED_COUNT/2,brightness8);
          }
        }*/
        float brightness;
        float accel=(imu.get_accel(1)-prev_accel)*2.0;//imu.get_accel(1);
        float gyro=imu.get_gyro(2)/100.0;
        if(iter<CHARLIPLEX_LED_COUNT/2)
        {
          brightness=gyro*.3+.7*accel+imu.get_accel(1)*.75;
        }else{
          brightness=gyro*.7+.3*accel+imu.get_accel(1)*.75;
        }
        brightness=max(brightness,-1.0);
        brightness=min(brightness,1.0);
        brightness=(0.25-brightness/4)*CHARLIPLEX_LED_COUNT;
        brightness=255*(1.4-abs(brightness-(iter%(CHARLIPLEX_LED_COUNT/2)))/3.0);
        brightness=max(brightness,0);
        brightness=min(brightness,255);
        uint8_t brightness8=(uint8_t)brightness;
        //led_lower.set_brightness(iter,brightness8);
        led_lower.set_brightness(iter,brightness8);
      }
    }
    led_upper.flush();
    led_lower.flush();
    prev_accel=prev_accel*0.997+0.003*imu.get_accel(1);
}


#define SSD1327_BUFFER_SIZE 128*128/2

void placeholder_spi()
{
  uint8_t* tx_buffer=get_buffer(0);
  for (int i = 0; i < SSD1327_BUFFER_SIZE; i++) {
      tx_buffer[i]+=0x0202; //animation
  }
  flush();
  draw();
}


int _dma_tx_channel;
uint8_t _tx_buffer[2][SSD1327_BUFFER_SIZE];
bool _display_index=0;//which index is being written to the hardware

const uint8_t frame_command_buffer[]={
                   SSD1327_SETROW,    0, 0x7F,
                   SSD1327_SETCOLUMN, 0, 0x3F};
                   const uint8_t init_128x128[] = {
      // Init sequence for 128x32 OLED module
      SSD1327_DISPLAYOFF, // 0xAE
      SSD1327_SETCONTRAST,
      0x80,             // 0x81, 0x80
      SSD1327_SEGREMAP, // 0xA0 0x53
      0x51, // remap memory, odd even columns, com flip and column swap
      SSD1327_SETSTARTLINE,
      0x00, // 0xA1, 0x00
      SSD1327_SETDISPLAYOFFSET,
      0x00, // 0xA2, 0x00
      SSD1327_DISPLAYALLOFF, SSD1327_SETMULTIPLEX,
      0x7F, // 0xA8, 0x7F (1/64)
      SSD1327_PHASELEN,
      0x11, // 0xB1, 0x11
      /*
      SSD1327_GRAYTABLE,
      0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06,
      0x07, 0x08, 0x10, 0x18, 0x20, 0x2f, 0x38, 0x3f,
      */
      SSD1327_DCLK,
      0x00, // 0xb3, 0x00 (100hz)
      SSD1327_REGULATOR,
      0x01, // 0xAB, 0x01
      SSD1327_PRECHARGE2,
      0x04, // 0xB6, 0x04
      SSD1327_SETVCOM,
      0x0F, // 0xBE, 0x0F
      SSD1327_PRECHARGE,
      0x08, // 0xBC, 0x08
      SSD1327_FUNCSELB,
      0x62, // 0xD5, 0x62
      SSD1327_CMDLOCK,
      0x12, // 0xFD, 0x12
      SSD1327_NORMALDISPLAY, SSD1327_DISPLAYON};

const uint8_t contrast_command_buffer_1[]={SSD1327_DISPLAYON};
const uint8_t contrast_command_buffer_2[]={0x81,0x2F};

void placeholder_begin(){
  for(uint8_t demo=0;demo<2;demo++)
  {
    uint8_t* tx_buffer=get_buffer(0);
    for (int i = 0; i < SSD1327_BUFFER_SIZE; i++) {
        tx_buffer[i] = (i + (demo?0x0101:0)) % 256; 
    }
    flush();
  }
  //claim spi channel, init spi pins
  
  // --- DMA Setup ---
  _dma_tx_channel = dma_claim_unused_channel(true);

  dma_channel_config c = dma_channel_get_default_config(_dma_tx_channel);
  channel_config_set_transfer_data_size(&c, DMA_SIZE_8); // 8-bit transfers
  channel_config_set_read_increment(&c, true); // Increment read address (source buffer)
  channel_config_set_write_increment(&c, false); // Don't increment write address (SPI data register is a fixed address)
  // Set the DREQ for SPI0 TX to automatically trigger transfers
  channel_config_set_dreq(&c, spi_get_dreq(spi1, true)); 

  // Configure the DMA channel, but don't start it yet
  dma_channel_configure(
      _dma_tx_channel,
      &c,
      &spi_get_hw(spi1)->dr, // Destination: SPI Data Register
      _tx_buffer[_display_index],// Source: our data buffer
      SSD1327_BUFFER_SIZE,       // Number of transfers
      false                      // Don't start immediately
  );

  delay(100);//need >30ms for screen to boot up stable, otherwise comes up with inverted or offset colors (?)
  send_data_dma(init_128x128, sizeof(init_128x128),false);
  delay(100);
  send_data_dma(contrast_command_buffer_1, sizeof(contrast_command_buffer_1),false);
  send_data_dma(contrast_command_buffer_2, sizeof(contrast_command_buffer_2),false);
}
void flush(){//push buffer to hardware
  _display_index^=1;
  draw();
}
void draw(){//update hardware
  send_data_dma(frame_command_buffer, sizeof(frame_command_buffer),false);
  send_data_dma(_tx_buffer[_display_index], SSD1327_BUFFER_SIZE,true);//dc true only for frame data
}
uint8_t* get_buffer(){ return get_buffer(1); }
uint8_t* get_buffer(bool is_black){//pointer to buffer that can be written into.  can set to all zeros with is_black=true (returns stale values if is_black=false)
  if(is_black)
  {
    for (int i = 0; i < SSD1327_BUFFER_SIZE; i++)
      _tx_buffer[!_display_index][i]=0;
  }
  return _tx_buffer[!_display_index];
}
//TODO: refactor without blocking calls...
void send_data_dma(const uint8_t *data, size_t len, bool dc_value) {
    // Ensure the previous DMA transfer is complete
    dma_channel_wait_for_finish_blocking(_dma_tx_channel);
    while (spi_is_busy(spi1));

    // Set Data/Command line to Data mode (if required by your OLED)
    gpio_put(SPI1_DC,dc_value);

    // Pull CS low to begin transaction
    gpio_put(SPI1_CS, LOW);

    // Reconfigure the DMA transfer count for the current data length
    dma_channel_set_read_addr(_dma_tx_channel, data, false);
    dma_channel_set_trans_count(_dma_tx_channel, len, false);
    
    // Start the DMA transfer
    dma_channel_start(_dma_tx_channel);

    // Wait for the DMA transfer to complete without blocking the CPU
    // The CPU can do other tasks here if needed
    dma_channel_wait_for_finish_blocking(_dma_tx_channel);
    while (spi_is_busy(spi1));
    // Pull CS high to end transaction
    gpio_put(SPI1_CS, HIGH);

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
