#include "led.h"

uint Charlieplex::_sm_offset = -1; //define static (upload pio program only one time)

uint16_t const CHARLIEPLEX_PINOUT_CONFIG[2][CHARLIEPLEX_LED_COUNT]={//index 0: lower LEDs config (under screen), index 1: upper LEDs (in hair)
{//lower LEDs
//red [0..23] left-to-right from led_matrix.ods.  MSB is dir (output=1, float=0), LSB is pin (1=high, 0=low)
0x6040,0x4840,0x0A02,0x2202,0x8202,0x4202,0x0901,0x2101,0x4101,0x1101,0x0301,0x8101,0x6020,0x4808,0x0A08,0x2220,0x4240,0x8280,0x8180,0x4140,0x2120,0x0908,0x1110,0x0302,

//green 24..47 left-to-right
0xA080,0x8880,0x1810,0x3010,0x9010,0x5010,0x0C04,0x2404,0x4404,0x1404,0x0604,0x8404,0xA020,0x8808,0x1808,0x3020,0x5040,0x9080,0x8480,0x4440,0x2420,0x0C08,0x1410,0x0602
},{//upper LEDs
//red CW around her face, then mostly left-to-right (and bottom-to-top along diagonals)
0x0301,0x1101,0x8280,0x9080,0x8180,0xA080,0x0604,0x1404,0x0504,0x2404,0x4404,0x8404,0x0302,0x1110,0x8202,0x9010,0x8101,0xA020,0x0602,0x1410,0x0501,0x2420,0x4440,0x8480,

//green
0x2220,0x3020,0x4240,0x5040,0x4140,0x6040,0x0A08,0x1808,0x0908,0x2808,0x4808,0x8808,0x2202,0x3010,0x4202,0x5010,0x4101,0x6020,0x0A02,0x1810,0x0901,0x2820,0x4840,0x8880
}};

Charlieplex::Charlieplex(bool is_upper)
{
  _pio_index=is_upper;
  _first_pin=is_upper?16:0;
  //_current_list_ptr = _charlieplex_list[0];
  _current_list_ptr = &_charlieplex_list[0][0]; 
}

void Charlieplex::begin(){
    if(_pio_index)
    {
      for(int iter=16;iter<24;iter++){ gpio_init(iter);  gpio_set_dir(iter, GPIO_IN); gpio_disable_pulls(iter); }
    }else{
      for(int iter=0;iter<8;iter++){ gpio_init(iter);  gpio_set_dir(iter, GPIO_IN); gpio_disable_pulls(iter); }
    }

    // 1. Initialize PIO
    if(_sm_offset == (uint)-1)
      _sm_offset = pio_add_program(_pio, &charlieplex_dma_program);//load once in main

    _sm = pio_claim_unused_sm(_pio, true);//for each charlieplex LED (one for lower, one for upper)
    charlieplex_dma_program_init(_pio, _sm, _sm_offset, _first_pin, 8);

    // 2. Configure DATA DMA (The worker)
    _data_chan = dma_claim_unused_channel(true);
    dma_channel_config c_data = dma_channel_get_default_config(_data_chan);
    channel_config_set_transfer_data_size(&c_data, DMA_SIZE_32);
    channel_config_set_read_increment(&c_data, true);
    channel_config_set_write_increment(&c_data, false);
    channel_config_set_dreq(&c_data, pio_get_dreq(_pio, _sm, true));
    
    // 3. Configure CONTROL DMA (The restarter)
    _ctrl_chan = dma_claim_unused_channel(true);
    dma_channel_config c_ctrl = dma_channel_get_default_config(_ctrl_chan);
    channel_config_set_transfer_data_size(&c_ctrl, DMA_SIZE_32);
    channel_config_set_read_increment(&c_ctrl, false);
    channel_config_set_write_increment(&c_ctrl, false);

    // CHAIN: Data finishing triggers Control
    channel_config_set_chain_to(&c_data, _ctrl_chan);

    dma_channel_configure(
        _data_chan, &c_data,
        &_pio->txf[_sm],     
        _current_list_ptr,  
        CHARLIEPLEX_LED_COUNT+1,                
        false              
    );

    dma_channel_configure(
        _ctrl_chan, &c_ctrl,
        &dma_hw->ch[_data_chan].al3_read_addr_trig, // Restart Data Channel
        &_current_list_ptr,                         // By reading the pointer variable
        1,
        false
    );

    //setup buffer (move 16-bit pinout constants into 2x 32-bit buffers)
    for(uint8_t frame_buffer_index=0;frame_buffer_index<2;frame_buffer_index++)
    {
        for(uint8_t led_index=0;led_index<CHARLIEPLEX_LED_COUNT;led_index++)
            _charlieplex_list[frame_buffer_index][led_index]=CHARLIEPLEX_PINOUT_CONFIG[_pio_index][led_index];
        _charlieplex_list[frame_buffer_index][CHARLIEPLEX_LED_COUNT]=0;//not strictly needed since populated in update(), but included for completeness: final wait statement to stabalize timing for a single frame
    }

    dma_channel_start(_ctrl_chan);//move to stand-alone method as sparation of concerns between init and run?
}

bool Charlieplex::flush()
{
    // Use a different index than the one currently being displayed by DMA
    uint8_t write_index = (_charlieplex_index + 1) % 2;
    uint32_t darkness=_max_effective_led_count*255*255*4/5;//80% max brightness to get 60 Hz update with 48 LED elements
    for(uint8_t led_index=0;led_index<CHARLIEPLEX_LED_COUNT;led_index++)
    {
        uint16_t brightness = _api_brightness[led_index];
        brightness=((uint32_t)(brightness*brightness))*4/5;//80% max brightness to get 60 Hz update with 48 LED elements
        _charlieplex_list[write_index][led_index]=((uint16_t)_charlieplex_list[write_index][led_index]) | (brightness<<16);//keep the old 16 LSbits about pin directions, and put the brightness in the upper 16 bits.
        _api_brightness[led_index]=0;//reset api to default value
    }
    if(darkness>(_max_effective_led_count*255*255*4/5)) darkness=0;//resolve any roll-over of LEDs being cumulatively brighter than allowed
    _charlieplex_list[write_index][CHARLIEPLEX_LED_COUNT]=darkness<<8;//put 24-bit darkness value as final element, with 0 LEDs drive (LSB) to indicate it is a simple wait
    _current_list_ptr = _charlieplex_list[write_index];
    _charlieplex_index = write_index;
    //dma_channel_set_read_addr(_data_chan, _current_list_ptr, true);
    return true;
}

bool Charlieplex::set_brightness(uint8_t index,uint8_t brightness)
{
  if(index>(sizeof(_api_brightness)/sizeof(_api_brightness[0]))) return false;
  _api_brightness[index]=brightness;
  return true;
}

bool Charlieplex::set_effective_led_count(uint8_t count)
{//used to calculate the blanking interval at the end of a frame to keep the brightness stable while minimizing flickering
  if(count==0) return false;
  _max_effective_led_count=count;
  return true;
}
