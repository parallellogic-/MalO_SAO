#include "combo1.h"
#include "hardware/gpio.h"
#include "hardware/pio.h"
#include "hardware/structs/padsbank0.h"

SSD1327 ssd1327(9,8,11,10,spi1,8'000'000*4);
Charlieplex charlieplex_lower(0);
Charlieplex charlieplex_upper(1);

uint8_t DEBUG_LED_PIN_R=37;
uint8_t DEBUG_LED_PIN_G=38;
bool is_tone=false;

int debug_led_index=0;
bool is_down=false;

void setup() {
  //padsbank0_hw->io[0] = PADS_BANK0_GPIO0_IE_BITS; 
  //Serial.begin(1'000'000);
  //fix green leakage ghosting:
  //padsbank0_hw->io[0] = PADS_BANK0_GPIO0_OD_BITS | PADS_BANK0_GPIO0_IE_BITS; 
  //gpio_pull_up(0);
  //hw_clear_bits(&padsbank0_hw->io[0], PADS_BANK0_GPIO0_PUE_BITS);
  //hw_clear_bits(&padsbank0_hw->io[0], PADS_BANK0_GPIO0_PDE_BITS);
  for(int iter=0;iter<8;iter++){ pinMode(iter,INPUT); gpio_disable_pulls(iter); }
  for(int iter=16;iter<24;iter++) pinMode(iter,INPUT);
  //for(int iter=0;iter<64;iter++){ pinMode(iter,INPUT); gpio_disable_pulls(iter); }

  //debug_arc();
  //gpio_init(0);
  /*pinMode(0,OUTPUT);
  pinMode(3,OUTPUT);
  digitalWrite(0,1);
  digitalWrite(3,0);
  while(1);*/

  gpio_set_dir(DEBUG_LED_PIN_R, GPIO_OUT);
  gpio_set_dir(DEBUG_LED_PIN_G, GPIO_OUT);
  gpio_set_dir(40, GPIO_OUT);
  gpio_set_dir(25, GPIO_OUT);

  ssd1327.begin();
  for(uint8_t demo=0;demo<2;demo++)
  {//fill 2 buffers with default values
    uint8_t* tx_buffer=ssd1327.get_buffer(0);
    for (int i = 0; i < SSD1327_BUFFER_SIZE; i++) {
        tx_buffer[i] = (i + (demo?0x0101:0)) % 256; 
    }
    ssd1327.flush();
  }
  charlieplex_upper.begin();
  charlieplex_lower.begin();


    /*gpio_set_dir(0, GPIO_IN);
    gpio_put(0, 0);
    gpio_set_function(0, GPIO_FUNC_NULL);
    gpio_set_function(0, GPIO_FUNC_SIO);
  gpio_disable_pulls(0);
  pio_gpio_init(pio0, 0);
    // 1. Let Arduino initialize the base underlying pad structures
  pinMode(0, OUTPUT); 

  // 2. Hand ownership of the physical pin away from SIO and over to PIO0
  pio_gpio_init(pio0, 0);

  // 3. Configure the PIO direction to output using the correct function signature
  // Syntax: (instance, state_machine, pin_direction_bits, target_mask)
  pio_sm_set_pindirs_with_mask(pio0, 0, (1<<0), (1<<0));

  // 4. Force the PIO block to drive the physical pin High
  // Syntax: (instance, state_machine, pin_value_bits, target_mask)
  pio_sm_set_pins_with_mask(pio0, 0, (1<<0), (1<<0));*/
}

uint32_t last_print_tms=0;

void loop() {
  /*if((millis()-last_print_tms)>1000)
  {
    last_print_tms=millis();
    //Serial.println("HERE");
    Serial.print(charlieplex_lower._current_list_ptr[6],HEX);
    Serial.print("\t");
    Serial.println(charlieplex_upper._current_list_ptr[6],HEX);
  }*/

  absolute_time_t start_time = get_absolute_time();
  gpio_put(DEBUG_LED_PIN_R, (millis()%1000)>500);

  // -- ssd1327 oled --
  uint8_t* tx_buffer=ssd1327.get_buffer(0);
  for (int i = 0; i < SSD1327_BUFFER_SIZE; i++) {
      tx_buffer[i]+=0x0202; //animation
  }

  // -- cap touch --
  //placeholder manual cap touch ~12 ms
  for(int pin=28;pin<=36;pin++)
  {
    int run_sum=0;
    for(int iter=0;iter<50;iter++)
    {
      pinMode(pin,OUTPUT);
      digitalWrite(pin,HIGH);
      int counter=0;
      pinMode(pin,INPUT);
      while(true)
      {
        counter++;
        if(!digitalRead(pin))
        {
          run_sum+=counter;
          break;
        }
      }
    }
    run_sum/=50;
    switch(pin)
    {
      case 28: run_sum-=79; break; 
      case 29: run_sum-=88; break; 
      case 30: run_sum-=99; break; 
      case 31: run_sum-=78; break; 
      case 32: run_sum-=76; break; 
      case 33: run_sum-=98; break; 
      case 34: run_sum-=84; break; 
      case 35: run_sum-=94; break; 
      case 36: run_sum-=95; break; 
    }
    run_sum+=20;
    //Serial.print(pin);
    //Serial.print(", ");
    //Serial.println(run_sum);
    uint8_t pin_row=(pin-28)/3;
    uint8_t pin_col=(pin-28)%3;
    bool is_press=0;
    run_sum=(run_sum>>6)&0x0F;
    for (int i = 0; i < SSD1327_BUFFER_SIZE; i++) {
      uint8_t px_row=(i%64)*2;
      uint8_t px_col=127-i/64;
      if(px_row>(32*pin_row) && px_row<(32*(pin_row+1)) &&
         px_col>(32*pin_col) && px_col<(32*(pin_col+1)))
      {
        //if(run_sum>40)tx_buffer[i]=0xFF;
        //else tx_buffer[i]=0x00;
        tx_buffer[i]=run_sum|(run_sum<<4);
        is_press=run_sum>0;
      }
    }
    switch(pin)
    {
      case 28:{
        digitalWrite(40,is_press);
      }break;
      case 30:{
        if(is_press){ if(!is_tone){ is_tone=true; tone(25, 440); } }
        else noTone(25);
        is_tone=is_press;
      }break;
      case 36:{
        if(is_press && !is_down)
        {
          is_down=true;
          debug_led_index++;
          debug_led_index%=48;
        }else if(!is_press && is_down) is_down=false;
      }break;
    }
  }

  // -- led --
  charlieplex_upper.set_max_effective_led_count(CHARLIPLEX_LED_COUNT/2);
  charlieplex_lower.set_max_effective_led_count(7);

  for(uint8_t iter=0;iter<CHARLIPLEX_LED_COUNT;iter++)
  {
    //slow fade
    uint16_t brightness_upper = (-millis()/8)+iter*32; 
    if(brightness_upper & 0x0100) brightness_upper=255-(uint8_t)brightness_upper;//fade fully off half the time
    charlieplex_upper.set_brightness(iter,(uint8_t)brightness_upper);

    //all LEDs ON dimly, but one much brighter
    int8_t brightness_lower = millis()/16000%2?0:32*2; 
    brightness_lower=iter<24?brightness_lower:brightness_lower/2;
    if((((iter%24)%5)==(millis()/(64*4))%5)) brightness_lower=iter<24?255:128;
    charlieplex_lower.set_brightness(iter,brightness_lower);

    //uint8_t brightness_lower = 0; 
    //if(iter==debug_led_index) brightness_lower=255;
    //charlieplex_lower.set_brightness(iter,brightness_lower);

  }
  charlieplex_upper.flush();
  charlieplex_lower.flush();

  // -- ssd1327 oled --
  ssd1327.flush();

  //Serial.println(get_absolute_time()-start_time);//10.7msat 8 MHz, 5.5ms at 16MHz, 3.4 ms at 32 MHz
  sleep_until(delayed_by_us(start_time, 16'666));
}
