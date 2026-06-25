#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "core1_main.h"
#include "dma_control_block.h"
#include "screen.h"

#define LED_CORE1_PIN 38 //debug green

#define SPI1_CS 9
#define SPI1_DC 8
#define SPI1_MOSI 11
#define SPI1_SCLK 10
#define SPI1_BAUD 8'000'000

Charlieplex led_lower(false);
Charlieplex led_upper(true);
Screen screen(spi1,SPI1_BAUD,SPI1_DC);

volatile uint32_t is_core1_halt = 0;
volatile uint32_t frame_id = 0;

ScatterGatherEngine scatterer_gatherer_engine_general;
ScatterGatherEngine scatterer_gatherer_engine_screen;

void core1_begin(){
  is_core1_halt = 0;
  frame_id = 0;

  //Serial.println("Init Scatterer Gatherer...");
  scatterer_gatherer_engine_general.begin(true); //I2C needs aux channels to perform sync'd reads.  also uses sniff0 to compute the length of the imu fifo
  scatterer_gatherer_engine_screen.begin(false); //limit to only 2 channels for screen

  //Serial.println("Init LEDs...");
  led_upper.begin();
  led_lower.begin();
  
  //Serial.println("Init SPI...");
  spi_init(spi1, SPI1_BAUD);
  gpio_set_function(SPI1_SCLK, GPIO_FUNC_SPI);
  gpio_set_function(SPI1_MOSI, GPIO_FUNC_SPI);
  gpio_set_function(SPI1_CS,   GPIO_FUNC_SPI);
  gpio_init(SPI1_DC);//is needed for proper screen operation
  gpio_set_dir(SPI1_DC, GPIO_OUT);
  gpio_put(SPI1_DC,1);
  
  //Serial.println("Init Screen...");
  screen.begin();
  scatterer_gatherer_engine_screen.registerSource(&screen);//50mA@5V
  
  //Serial.println("Finish Init Scatterer Gatherer...");
  scatterer_gatherer_engine_general.registerSource(&scatterer_gatherer_engine_general);
  scatterer_gatherer_engine_screen.registerSource(&scatterer_gatherer_engine_screen);//register self to perform end-of-cycle completion check
}

// This function runs exclusively on Core 1
void malo_core1_entry(void) {
    core1_begin();
    
    gpio_init(LED_CORE1_PIN);
    gpio_set_dir(LED_CORE1_PIN, GPIO_OUT);

    uint32_t previous_frame_id=0xFFFF'FFFF;
    bool is_frame_start=true;
    // Infinite blink loop for Core 1
    while (!is_core1_halt) {
      gpio_put(LED_CORE1_PIN, (to_ms_since_boot(get_absolute_time()) % 600) < 300);
      if(previous_frame_id!=frame_id)
      {//when aysnc process from core0 update frame_id, proceed to do next frame action
        is_frame_start=true;
        previous_frame_id=frame_id;
      }
      if(is_frame_start)
      {
        //scatterer_gatherer_engine_general.compileAndRun(frame_id);
        scatterer_gatherer_engine_screen.compileAndRun(frame_id);
        is_frame_start=false;
      }
      
    }
}

uint8_t* get_screen_buffer(){ return screen.get_frame_buffer(); }
void screen_flush(){ screen.flush(); }
//void set_screen_enabled(bool is_enabled){ screen.set_enabled(is_enabled); }

extern "C" {

bool set_charlieplex_led(uint8_t bank_index,uint8_t led_index,uint8_t brightness)
{
  if(bank_index>=2) return false;
  if(bank_index==1) return led_upper.set_brightness(led_index,brightness);
  return led_lower.set_brightness(led_index,brightness);
}

bool set_effective_led_count(uint8_t bank_index,uint8_t led_count)
{
  if(bank_index>=2) return false;
  if(bank_index==1) return led_upper.set_effective_led_count(led_count);
  return led_lower.set_effective_led_count(led_count);
}

bool flush(uint8_t bank_index)
{
  if(bank_index>=2) return false;
  if(bank_index==1) return led_upper.flush();
  return led_lower.flush();
}

} // extern "C"
