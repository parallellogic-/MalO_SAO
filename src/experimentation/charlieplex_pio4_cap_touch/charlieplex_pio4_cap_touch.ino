//charliplexes leds using PIO (alternates between driving high/low and floating pins)
//the PIO can drive a max of 8*7=56 leds using 8 IO pins with 16-pin depth
//At maximum capability, this would be 40 FPS (150MHz/(0xFFFF*8*7)).
//To reach 60 FPS, this application is tuned to 24 red-green LEDs (48 led elements)
//and the brightness is scaled down 20% (only using 14 bits)
//for simplicity, the API allows setting an 8-bit brightness per LED.
//this brightness is squared (to align with the log-scale sensitivty of the human eye)
//and scaled to 80% of the result value --> 14 bits.
//a silent final 24-bit delay state is used at the end of the pio dma to allow the apparent brightness
//of the entire display to remain cosntant, independnet of the brightness of individual LED elements.
//This solution uses 2 DMAs.  One to load the PIO state machine with pin high/low (8 bits), pin input(float)/output (8 bits), and delay (16 bits)
//the other DMA is used to reload (loop) the first.  Alternative archtiectures considered were a single DMA
//which requires a multiple-of-2 memory size and must immediately change DMA addresses on an upate
//(which results in a glitch every time the memory is changed, and ruled unusable).  An IRQ can be used for the loop update, but trying
//to avoid using interrupts unless absolutely necessary because it's a more scarce resource and slightly less deterministic
//TODO: Command to set effective number of LEDs - sets delay statement max value

#include <hardware/pio.h>
#include <hardware/dma.h>
#include "charlie.pio.h"
#include "hardware/regs/pads_bank0.h"
#include "hardware/structs/padsbank0.h"

#include "hardware/pio.h"
#include "hardware/dma.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"
#include "logic_analyzer.pio.h"
#include "hardware/regs/sysinfo.h"
#include "hardware/regs/addressmap.h"
#include <stdio.h>
#include "pico/stdlib.h"

// -- cap touch --

uint8_t FIRST_PIN_CAPTOUCH=26;
uint8_t PIN_COUNT_CAPTOUCH=2;//11;//max as implemented in PIO is 11 (1 PWM, 10 cap touch) to allow maximal number of bits for time counter
int INDICATOR_LED=37;

// RP2350 DMA Ring alignment must match buffer size in bytes (1024 * 4 = 4096)
//0x0fff is max last_read_idx value
#define RING_BUFFER_SIZE 4096//1024
uint32_t capture_buffer[RING_BUFFER_SIZE] __attribute__((aligned(16384)));//4096)));

int dma_chan;
uint32_t last_read_idx = 0;
bool last_gpio10_state = false;
uint32_t last_press=0;
uint16_t last_touch_state=0;

// -- charlieplex led --

//uint8_t const FIRST_PIN=0;
//uint8_t const PINOUT_MAPPING_INDEX=0;//PCB layout routing made LEDs non-sequential between how they are commanded and how they appear.  This mapping corrects that.  Value is which pinout (for lower LEDs under screen =0, or upper LEDs in hair =1)

uint8_t const FIRST_PIN=16;
uint8_t const PINOUT_MAPPING_INDEX=1;//PCB layout routing made LEDs non-sequential between how they are commanded and how they appear.  This mapping corrects that.  Value is which pinout (for lower LEDs under screen =0, or upper LEDs in hair =1)

uint8_t const PIO_CHARLIPLEX_LED_COUNT=24*2;//24 reg-green LEDs
uint8_t const PIO_FRAME_BUFFER_DEPTH=2;//one being read from by PIO, one being buffered into - then ping-pong swap after flush()
uint8_t api_brightness[PIO_CHARLIPLEX_LED_COUNT];//write brightness values here for the frame currently being developed (red in lower 24 indexes, green in the upper 24)
uint32_t pio_charliplex_list[PIO_FRAME_BUFFER_DEPTH][PIO_CHARLIPLEX_LED_COUNT+1];//31..16 is brightness, 15..8 is input(0) vs output(1) for 8 pins, 7..0 is high(1) vs low(0) for 8 pins.  +1 for a sleep statement for brightness stabalization
//uint32_t MAX_DARKNESS=PIO_CHARLIPLEX_LED_COUNT*255*255*4/5;
uint8_t max_effective_led_count=PIO_CHARLIPLEX_LED_COUNT;
uint8_t pio_charliplex_index = 0;//the buffer index being read from for the current DMA operation
uint32_t* current_list_ptr = pio_charliplex_list[0]; // The "Next" list pointer

uint16_t const PINOUT_CONFIG[2][PIO_CHARLIPLEX_LED_COUNT]={//index 0: lower LEDs config (under screen), index 1: upper LEDs (in hair)
{//lower LEDs
//red [0..23] left-to-right from led_matrix.ods.  MSB is dir (output=1, float=0), LSB is pin (1=high, 0=low)
0x4060,
0x4048,
0x020A,
0x0222,
0x0282,
0x0242,
0x0109,
0x0121,
0x0141,
0x0111,
0x0103,
0x0181,
0x2060,
0x0848,
0x080A,
0x2022,
0x4042,
0x8082,
0x8081,
0x4041,
0x2021,
0x0809,
0x1011,
0x0203,

//green 24..47 left-to-right
0x80A0,
0x8088,
0x1018,
0x1030,
0x1090,
0x1050,
0x040C,
0x0424,
0x0444,
0x0414,
0x0406,
0x0484,
0x20A0,
0x0888,
0x0818,
0x2030,
0x4050,
0x8090,
0x8084,
0x4044,
0x2024,
0x080C,
0x1014,
0x0206

},{//upper LEDs
//red CW around her face, then mostly left-to-right (and bottom-to-top along diagonals)
0x0103,
0x0111,
0x8082,
0x8090,
0x8081,
0x80A0,
0x0406,
0x0414,
0x0405,
0x0424,
0x0444,
0x0484,
0x0203,
0x1011,
0x0282,
0x1090,
0x0181,
0x20A0,
0x0206,
0x1014,
0x0105,
0x2024,
0x4044,
0x8084,

//green
0x2022,
0x2030,
0x4042,
0x4050,
0x4041,
0x4060,
0x080A,
0x0818,
0x0809,
0x0828,
0x0848,
0x0888,
0x0222,
0x1030,
0x0242,
0x1050,
0x0141,
0x2060,
0x020A,
0x1018,
0x0109,
0x2028,
0x4048,
0x8088

}};

PIO pio = pio0;
uint sm;
int data_chan;
int ctrl_chan;

void setup() {
    Serial.begin(115200);
    // Initialize Built-in LED
    //pinMode(LED_BUILTIN, OUTPUT);

    // -- cap touch --


    // 1. PWM Heartbeat on LED (10Hz)
    //setup_pwm(LED_BUILTIN, 10.0f); 
    pinMode(INDICATOR_LED,OUTPUT); digitalWrite(INDICATOR_LED,0);

    // 2. Generate Pilot Signals
    //setup_pwm(10, 10.0f);           
    /*pinMode(10,OUTPUT); digitalWrite(10,0);
    //setup_pwm(11, 10.0f * PI);      
    //setup_pwm(11, PI);      
    pinMode(11,OUTPUT); digitalWrite(11,0);
    //setup_pwm(12, 10.0f * 2.71828f);
    pinMode(12,OUTPUT); digitalWrite(12,0);
    pinMode(13,OUTPUT); digitalWrite(13,0);
    pinMode(14,OUTPUT); digitalWrite(14,0);
    pinMode(15,OUTPUT); digitalWrite(15,0);*/

    // 3. PIO Setup
    uint offset2 = pio_add_program(pio1, &logic_analyzer_program);
    uint sm = 0;


    // Enable input buffers so PIO can "see" the PWM signals
    for (int i = FIRST_PIN_CAPTOUCH; i < FIRST_PIN_CAPTOUCH+PIN_COUNT_CAPTOUCH; i++) {
        //pinMode(i,OUTPUT);
        //digitalWrite(i,0);
        gpio_disable_pulls(i);
        pio_gpio_init(pio1, i);
        gpio_set_input_enabled(i, true);
        gpio_disable_pulls(i);
    }

    pinMode(FIRST_PIN_CAPTOUCH,OUTPUT);
    digitalWrite(FIRST_PIN_CAPTOUCH,0);//only first pin is drive PWM

    pio_sm_config c = logic_analyzer_program_get_default_config(offset2);
    sm_config_set_in_pins(&c, FIRST_PIN_CAPTOUCH); //start at gpio 10
    sm_config_set_in_pin_count(&c, PIN_COUNT_CAPTOUCH); //use 11 pins
    
    // RP2350 requires the FIFO to be joined to handle high-speed bursts
    sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_RX);

    pio_sm_init(pio1, sm, offset2, &c);
    pio_sm_set_enabled(pio1, sm, true);

    // 4. DMA Setup
    dma_chan = dma_claim_unused_channel(true);
    dma_channel_config dma_c = dma_channel_get_default_config(dma_chan);
    channel_config_set_transfer_data_size(&dma_c, DMA_SIZE_32);
    channel_config_set_read_increment(&dma_c, false);
    channel_config_set_write_increment(&dma_c, true);
    channel_config_set_ring(&dma_c, true, 10+2+2); // 10+2 bits = 1024*4 words --> +2 fudge factor needed (for uint8 to uint32 adaption?))
    channel_config_set_dreq(&dma_c, pio_get_dreq(pio1, sm, false));

    dma_channel_configure(dma_chan, &dma_c, capture_buffer, &pio1->rxf[sm], 0xFFFFFFFF, true);

    Serial.println("--- RP2350 Analyzer Online ---");

    if(true){//4khz
        // 1. Initialize the GPIO for PWM function
        gpio_set_function(FIRST_PIN_CAPTOUCH, GPIO_FUNC_PWM);

        // 2. Identify which PWM slice is connected to this pin
        uint slice_num = pwm_gpio_to_slice_num(FIRST_PIN_CAPTOUCH);

        // 3. Set the clock divider to 1.0 (No division)
        // This ensures the PWM counter increments exactly once per sys_clk cycle.

        if(true)
        {
          pwm_set_clkdiv(slice_num, 1.0f);
          // 4. Set the Wrap Value (TOP)
          // The frequency will be sys_clk / (WRAP + 1).
          // For a divisor of 24,000, set wrap to 23,999.
          pwm_set_wrap(slice_num, 37500/4-1);//150mhz / 37.5k = 4khz, if clock div=1.0

          // 5. Set the Duty Cycle to exactly 50%
          // Level should be half of (WRAP + 1)
          pwm_set_gpio_level(FIRST_PIN_CAPTOUCH, 37500/8);
        }
        if(false)
        {
          pwm_set_clkdiv(slice_num, 1.0f);
          pwm_set_wrap(slice_num, 999);//150mhz / 1k = 150khz
          pwm_set_gpio_level(FIRST_PIN_CAPTOUCH, 1000/2);
        }

        // 6. Start the PWM slice
        pwm_set_enabled(slice_num, true);
    }

    // -- charlieplex led --

    // 1. Initialize PIO
    uint offset = pio_add_program(pio, &charlie_dma_program);//load once in main
    //gpio_init(0);
    //gpio_set_function(0, GPIO_FUNC_SIO);
    //gpio_disable_pulls(0);

    sm = pio_claim_unused_sm(pio, true);//for each charlieplex LED (one for lower, one for upper)
    charlie_dma_program_init(pio, sm, offset, FIRST_PIN, 8); //8 pins -- issue
    //pio_gpio_init(pio, 0);

    gpio_pull_up(0);   // test
    gpio_disable_pulls(0);
    // Disable pulls
    gpio_disable_pulls(0);
    // Set strong drive
    gpio_set_drive_strength(0, GPIO_DRIVE_STRENGTH_12MA);
    // Optional but helpful:
    gpio_set_slew_rate(0, GPIO_SLEW_RATE_FAST);
    //  Critical: disable input buffer (reduces loading)
    padsbank0_hw->io[0] &= ~PADS_BANK0_GPIO0_IE_BITS;
    // Ensure output is enabled (should already be via PIO, but safe)
    padsbank0_hw->io[0] |= PADS_BANK0_GPIO0_OD_BITS;
    gpio_init(0);
    gpio_set_function(0, GPIO_FUNC_PIO0);
    gpio_disable_pulls(0);
    gpio_set_drive_strength(0, GPIO_DRIVE_STRENGTH_12MA);
    gpio_set_slew_rate(0, GPIO_SLEW_RATE_FAST);
    // Kill input buffer
    padsbank0_hw->io[0] &= ~PADS_BANK0_GPIO0_IE_BITS;

    // 2. Configure DATA DMA (The worker)
    data_chan = dma_claim_unused_channel(true);
    dma_channel_config c_data = dma_channel_get_default_config(data_chan);
    channel_config_set_transfer_data_size(&c_data, DMA_SIZE_32);
    channel_config_set_read_increment(&c_data, true);
    channel_config_set_write_increment(&c_data, false);
    channel_config_set_dreq(&c_data, pio_get_dreq(pio, sm, true));
    
    // 3. Configure CONTROL DMA (The restarter)
    ctrl_chan = dma_claim_unused_channel(true);
    dma_channel_config c_ctrl = dma_channel_get_default_config(ctrl_chan);
    channel_config_set_transfer_data_size(&c_ctrl, DMA_SIZE_32);
    channel_config_set_read_increment(&c_ctrl, false);
    channel_config_set_write_increment(&c_ctrl, false);

    // CHAIN: Data finishing triggers Control
    channel_config_set_chain_to(&c_data, ctrl_chan);

    dma_channel_configure(
        data_chan, &c_data,
        &pio->txf[sm],     
        current_list_ptr,  
        PIO_CHARLIPLEX_LED_COUNT+1,                
        false              
    );

    dma_channel_configure(
        ctrl_chan, &c_ctrl,
        &dma_hw->ch[data_chan].al3_read_addr_trig, // Restart Data Channel
        &current_list_ptr,                         // By reading the pointer variable
        1,
        false
    );

    //setup buffer (move 16-bit pinout cosntants into 2x 32-bit buffers)
    for(uint8_t frame_buffer_index=0;frame_buffer_index<PIO_FRAME_BUFFER_DEPTH;frame_buffer_index++)
    {
        for(uint8_t led_index=0;led_index<PIO_CHARLIPLEX_LED_COUNT;led_index++)
            pio_charliplex_list[frame_buffer_index][led_index]=PINOUT_CONFIG[PINOUT_MAPPING_INDEX][led_index];
        pio_charliplex_list[frame_buffer_index][PIO_CHARLIPLEX_LED_COUNT]=0;//not strictly needed, but  included for completeness
    }

    //pinMode(0,OUTPUT);
    //pinMode(7,OUTPUT);
    //digitalWrite(0,HIGH);
    //digitalWrite(7,LOW);
    //pinMode(0,INPUT);
    //pinMode(7,INPUT);
    //while(true);
    //gpio_init(0);
    //gpio_init(7);
    //gpio_set_dir(0, GPIO_OUT);
    //gpio_set_dir(7, GPIO_OUT);
    //gpio_put(0, 1);
    //gpio_put(7, 0);
    //while(1); // Stop here

    dma_channel_start(ctrl_chan);
}

void flush()
{
    // Use a different index than the one currently being displayed by DMA
    uint8_t write_index = (pio_charliplex_index + 1) % PIO_FRAME_BUFFER_DEPTH;
    //uint32_t darkness=MAX_DARKNESS;//brightness-stabalization sleep statement at end of charlieplex operation
    uint32_t darkness=max_effective_led_count*255*255*4/5;
    for(uint8_t led_index=0;led_index<PIO_CHARLIPLEX_LED_COUNT;led_index++)
    {
        uint16_t brightness = api_brightness[led_index];
        brightness=((uint32_t)(brightness*brightness))*4/5;//80% max brightness to get 60 Hz update with 48 LED elements
        pio_charliplex_list[write_index][led_index]=((uint16_t)pio_charliplex_list[write_index][led_index]) | (brightness<<16);//keep the old 16 LSbits about pin directions, and put the brightness in teh upper 16 bits.
        api_brightness[led_index]=0;//reset api to default value
    }
    if(darkness>(max_effective_led_count*255*255*4/5)) darkness=0;//resolve any roll-over of LEDs being cumulatively brighter than allowed
    pio_charliplex_list[write_index][PIO_CHARLIPLEX_LED_COUNT]=darkness<<8;//put 24-bit darkness value as final element, with 0 LEDs drive (LSB) to indicate it is a simple wait
    current_list_ptr = pio_charliplex_list[write_index];
    pio_charliplex_index = write_index;
}

void loop() {
    // -- cap touch --

    // 1. CONSTANTLY toggle pins so the PIO has something to see
    // This must be OUTSIDE the while(last_read_idx != current_idx) loop
    bool fast_toggle = (millis() / 1) % 2;
    bool slow_toggle = (millis() / 500) % 2;
    
    digitalWrite(FIRST_PIN_CAPTOUCH, fast_toggle); // 1Hz Pilot
    //digitalWrite(11, slow_toggle); // 50Hz Test
    //digitalWrite(12, slow_toggle);
    //digitalWrite(LED_BUILTIN, slow_toggle);

    // 2. Process whatever the PIO captured
    uint32_t write_addr = dma_hw->ch[dma_chan].write_addr;
    uint32_t current_idx = (write_addr - (uintptr_t)capture_buffer) / 4;
    //if (last_read_idx != current_idx) {
        while (last_read_idx != current_idx) {
            uint32_t data = capture_buffer[last_read_idx];
            //uint16_t pins = (data >> 21) & 0x7FF;
            //uint32_t counter_raw = data & 0x1FFFFF;
            uint16_t pins = data & 0x7FF;
            uint32_t counter_raw = data>>11;
            
            // Ticks elapsed = Start (0x1FFFFF) - End (counter_raw)
            //uint32_t elapsed_ticks = 0x1FFFFF - counter_raw;
            uint32_t elapsed_ticks = 0x200000 - counter_raw;

            String binString = String(pins, BIN);
            while(binString.length() < 11) binString = "0" + binString;

            // 150MHz / 5 cycles = 30,000 ticks per ms
            //Serial.printf("Edge! ms: %03.3f | Pins: %s\n", 
            //              (float)elapsed_ticks / 30000.0f, binString.c_str());
            Serial.printf("Edge! current_idx 0x%08x | last_read_idx 0x%08x | count: 0x%06x | Pins: %s\n",current_idx,last_read_idx, elapsed_ticks, binString.c_str());

            if((pins==0x0000) && (last_touch_state==0x0002))
            {//falling edge of captouch
                if(elapsed_ticks>0x000200)
                {
                    digitalWrite(INDICATOR_LED,HIGH);
                    last_press=millis();
                }
            }
            if((pins==0x0003) && (last_touch_state==0x0001))
            {//falling edge of captouch
                if(elapsed_ticks>0x000200)
                {
                    digitalWrite(INDICATOR_LED,HIGH);
                    last_press=millis();
                }
            }
            if((millis()-last_press)>50) digitalWrite(INDICATOR_LED,LOW);

            last_read_idx = (last_read_idx + 1) % RING_BUFFER_SIZE;
            last_touch_state=pins;
        }
    //}

    // -- charlieplex led --

    //delay(500);
    //Serial.println(pio_charliplex_list[0][6],HEX);
    //pio_diag();
    if(true)
    {
        uint8_t pattern_id=0;

        if(pattern_id==0)
        {
            max_effective_led_count=PIO_CHARLIPLEX_LED_COUNT/2;
        }else{
            max_effective_led_count=PIO_CHARLIPLEX_LED_COUNT;
        }

        for(uint8_t iter=0;iter<PIO_CHARLIPLEX_LED_COUNT;iter++)
        {
            if(pattern_id==0)
            {//slow fade
                uint16_t brightness = (millis()/8)+iter*32; 
                if(brightness & 0x0100) brightness=255-(uint8_t)brightness;//fade out half the time
                api_brightness[iter]=(uint8_t)brightness;
            }
            if(pattern_id==1)
            {//all LEDs ON dimly, but one much brighter
                uint8_t brightness = 32*2; 
                if((iter==(millis()/(64*4))%PIO_CHARLIPLEX_LED_COUNT)) brightness=255;
                api_brightness[iter]=brightness;
            }
            if(pattern_id==2)
            {
                uint8_t brightness = 0; 
                if(iter==(11+(millis()/3000)%2)) brightness=255;
                api_brightness[iter]=brightness;
            }
        }
        flush();
        //delay(16); 
        //digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    }
    //pio_diag();
}

#include "pio_diag.pio.h"

// --- Configuration ---
#define PIN_HIGH 0
#define PIN_LOW  7
// ---------------------

void pio_diag() {
    PIO pio = pio1;
    uint sm = pio_claim_unused_sm(pio, true);
    uint offset = pio_add_program(pio, &drive_pins_program);

    // 1. Initialize pins for PIO
    pio_gpio_init(pio, PIN_HIGH);
    pio_gpio_init(pio, PIN_LOW);

    // 2. Force pins to OUTPUT using a bitmask (handles non-consecutive pins)
    uint32_t pin_mask = (1u << PIN_HIGH) | (1u << PIN_LOW);
    pio_sm_set_pindirs_with_mask(pio, sm, pin_mask, pin_mask);

    // 3. Configure PIO
    pio_sm_config c = drive_pins_program_get_default_config(offset);
    
    // Set 'out' base to 0 so 'out pins, 32' maps bit 0 to GPIO0, bit 1 to GPIO1, etc.
    sm_config_set_out_pins(&c, 0, 32); 
    
    pio_sm_init(pio, sm, offset, &c);
    pio_sm_set_enabled(pio, sm, true);

    // 4. Create and send the bitmask
    // We want PIN_HIGH to be 1, and PIN_LOW to be 0
    uint32_t status_mask = (1u << PIN_HIGH); 
    pio_sm_put_blocking(pio, sm, status_mask);
    while(1);
}