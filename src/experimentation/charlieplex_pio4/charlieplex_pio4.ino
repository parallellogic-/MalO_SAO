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

#include <hardware/pio.h>
#include <hardware/dma.h>
#include "charlie.pio.h"

uint8_t const FIRST_PIN=2;
uint8_t const PINOUT_MAPPING_INDEX=1;//PCB layout routing made LEDs non-sequential between how they are commanded and how they appear.  This mapping corrects that.  Value is which pinout (for lower LEDs under screen =0, or upper LEDs in hair =1)

uint8_t const PIO_CHARLIPLEX_LED_COUNT=24*2;//24 reg-green LEDs
uint8_t const PIO_FRAME_BUFFER_DEPTH=2;//one being read from by PIO, one being buffered into - then ping-pong swap after flush()
uint8_t api_brightness[PIO_CHARLIPLEX_LED_COUNT];//write brightness values here for the frame currently being developed (red in lower 24 indexes, green in the upper 24)
uint32_t pio_charliplex_list[PIO_FRAME_BUFFER_DEPTH][PIO_CHARLIPLEX_LED_COUNT+1];//31..16 is brightness, 15..8 is input(0) vs output(1) for 8 pins, 7..0 is high(1) vs low(0) for 8 pins.  +1 for a sleep statement for brightness stabalization
uint32_t MAX_DARKNESS=PIO_CHARLIPLEX_LED_COUNT*255*255*4/5;
uint8_t pio_charliplex_index = 0;//the buffer index being read from for the current DMA operation
uint32_t* current_list_ptr = pio_charliplex_list[0]; // The "Next" list pointer

uint16_t const PINOUT_CONFIG[2][PIO_CHARLIPLEX_LED_COUNT]={//index 0: lower LEDs config (under screen), index 1: upper LEDs (in hair)
{//lower LEDs
0x6040,//red [0..23] left-to-right from led_matrix.ods.  MSB is dir (output=1, float=0), LSB is pin (1=high, 0=low)
0x4840,
0x0A02,
0x2202,
0x8202,
0x4202,
0x0901,
0x2101,
0x4101,
0x1101,
0x0301,
0x8101,
0x6020,
0x4808,
0x0A08,
0x2220,
0x4240,
0x8280,
0x8180,
0x4140,
0x2120,
0x0908,
0x1110,
0x0302,

0xA080,//green 24..47 left-to-right
0x8880,
0x1810,
0x3010,
0x9010,
0x5010,
0x0C04,
0x2404,
0x4404,
0x1404,
0x0604,
0x8404,
0xA020,
0x8808,
0x1808,
0x3020,
0x5040,
0x9080,
0x8480,
0x4440,
0x2420,
0x0C08,
0x1410,
0x0602

},{//upper LEDs

0x0301,//red CW around her face, then mostly left-to-right (and bottom-to-top along diagonals)
0x1101,
0x8280,
0x9080,
0x8180,
0xA080,
0x0604,
0x1404,
0x0504,
0x2404,
0x4404,
0x8404,
0x0302,
0x1110,
0x8202,
0x9010,
0x8101,
0xA020,
0x0602,
0x1410,
0x0501,
0x2420,
0x4440,
0x8480,

0x2220,//green
0x3020,
0x4240,
0x5040,
0x4140,
0x6040,
0x0A08,
0x1808,
0x0908,
0x2808,
0x4808,
0x8808,
0x2202,
0x3010,
0x4202,
0x5010,
0x4101,
0x6020,
0x0A02,
0x1810,
0x0901,
0x2820,
0x4840,
0x8880

}};

PIO pio = pio0;
uint sm;
int data_chan;
int ctrl_chan;

void setup() {
    // Initialize Built-in LED
    pinMode(LED_BUILTIN, OUTPUT);

    // 1. Initialize PIO
    uint offset = pio_add_program(pio, &charlie_dma_program);//load once in main
    sm = pio_claim_unused_sm(pio, true);//for each charlieplex LED (one for lower, one for upper)
    charlie_dma_program_init(pio, sm, offset, FIRST_PIN, 8);

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
        PIO_CHARLIPLEX_LED_COUNT,                
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
        for(uint8_t led_index=0;led_index<PIO_CHARLIPLEX_LED_COUNT;led_index++)
            pio_charliplex_list[frame_buffer_index][led_index]=PINOUT_CONFIG[PINOUT_MAPPING_INDEX][led_index];

    dma_channel_start(ctrl_chan);
}

void flush()
{
    // Use a different index than the one currently being displayed by DMA
    uint8_t write_index = (pio_charliplex_index + 1) % PIO_FRAME_BUFFER_DEPTH;
    uint32_t darkness=MAX_DARKNESS;//brightness-stabalization sleep statement at end of charlieplex operation
    for(uint8_t led_index=0;led_index<PIO_CHARLIPLEX_LED_COUNT;led_index++)
    {
        uint16_t brightness = api_brightness[led_index];
        brightness=((uint32_t)(brightness*brightness))*4/5;//80% max brightness to get 60 Hz update with 48 LED elements
        pio_charliplex_list[write_index][led_index]=((uint16_t)pio_charliplex_list[write_index][led_index]) | (brightness<<16);//keep the old 16 LSbits about pin directions, and put the brightness in teh upper 16 bits.
        api_brightness[led_index]=0;//reset api to default value
    }
    if(darkness>MAX_DARKNESS) darkness=0;//resolve any roll-over of LEDs being cumulatively brighter than allowed
    pio_charliplex_list[write_index][PIO_CHARLIPLEX_LED_COUNT]=darkness<<8;//put 24-bit darkness value as final element, with 0 LEDs drive (LSB) to indicate it is a simple wait
    current_list_ptr = pio_charliplex_list[write_index];
}

void loop() {
    if(true)
    {
        uint8_t pattern_id=1;

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
                uint8_t brightness = 32; 
                if((iter==(millis()/64)%PIO_CHARLIPLEX_LED_COUNT)) brightness=255;
                api_brightness[iter]=brightness;
            }
        }
        flush();
        delay(16); 
        digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    }
}
