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
        delay(16); 
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