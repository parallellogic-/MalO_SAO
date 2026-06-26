#include "led.h"

uint Charlieplex::_sm_offset = -1; //define static (upload pio program only one time)

Charlieplex::Charlieplex(bool is_upper)
{
  _pio_index=is_upper;
  _first_pin=is_upper?LED_LOWER_START_PIN:LED_UPPER_START_PIN;
  //_current_list_ptr = _charliplex_list[0];
  _current_list_ptr = &_charliplex_list[0][0]; 
}

void Charlieplex::begin(){
    for(int iter=_first_pin;iter<_first_pin+8;iter++){
      pinMode(iter,INPUT);
      gpio_disable_pulls(iter);
    }//init 8 contiguous LEDs as expected by PIO program

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
        CHARLIPLEX_LED_COUNT+1,                
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
        for(uint8_t led_index=0;led_index<CHARLIPLEX_LED_COUNT;led_index++)
            _charliplex_list[frame_buffer_index][led_index]=CHARLIEPLEX_PINOUT_CONFIG[_pio_index][led_index];
        _charliplex_list[frame_buffer_index][CHARLIPLEX_LED_COUNT]=0;//not strictly needed since populated in update(), but included for completeness: final wait statement to stabalize timing for a single frame
    }

    dma_channel_start(_ctrl_chan);//move to stand-alone method as sparation of concerns between init and run?
}

void Charlieplex::end() {
    // 1. CRITICAL STEP: Break the infinite DMA chaining sequence
    // Abort the control re-armer first so it can't restart the data engine
    if (_ctrl_chan >= 0) {
        dma_channel_abort(_ctrl_chan);
    }
    
    // Abort the data engine to instantly halt any mid-flight matrix updates
    if (_data_chan >= 0) {
        dma_channel_abort(_data_chan);
    }

    // 2. Halt the underlying PIO State Machine engine
    if (_sm != (uint)-1) {
        pio_sm_set_enabled(_pio, _sm, false);
        
        // Clear the PIO Tx FIFO to drain any lingering matrix transitions
        pio_sm_clear_fifos(_pio, _sm);
        
        // Release the state machine resource back to the Pico SDK pool
        pio_sm_unclaim(_pio, _sm);
        _sm = (uint)-1; // Mark as unallocated
    }

    // 3. Unclaim the DMA channels to clear them from the system bus matrix
    if (_ctrl_chan >= 0) {
        dma_channel_unclaim(_ctrl_chan);
        _ctrl_chan = -1;
    }
    if (_data_chan >= 0) {
        dma_channel_unclaim(_data_chan);
        _data_chan = -1;
    }

    // 4. Reset the instance's specific pins to a safe, high-impedance state
    // This isolates the Charlieplex grid from hardware noise during flash writes
    if (_pio_index) {
        // Upper Instance Pins (Pins 16-23)
        for (int iter = 16; iter < 24; iter++) {
            gpio_init(iter);
            gpio_set_dir(iter, GPIO_IN);
        }
    } else {
        // Lower Instance Pins (Pins 0-7)
        for (int iter = 0; iter < 8; iter++) {
            gpio_init(iter);
            gpio_set_dir(iter, GPIO_IN);
        }
    }
}

void Charlieplex::flush()
{
    // Use a different index than the one currently being displayed by DMA
    uint8_t write_index = (_charliplex_index + 1) % 2;
    uint32_t darkness=_max_effective_led_count*255*255*4/5;//80% max brightness to get 60 Hz update with 48 LED elements
    for(uint8_t led_index=0;led_index<CHARLIPLEX_LED_COUNT;led_index++)
    {
        uint16_t brightness = _api_brightness[led_index];
        brightness=((uint32_t)(brightness*brightness))*4/5;//80% max brightness to get 60 Hz update with 48 LED elements
        _charliplex_list[write_index][led_index]=((uint16_t)_charliplex_list[write_index][led_index]) | (brightness<<16);//keep the old 16 LSbits about pin directions, and put the brightness in the upper 16 bits.
        _api_brightness[led_index]=0;//reset api to default value
    }
    if(darkness>(_max_effective_led_count*255*255*4/5)) darkness=0;//resolve any roll-over of LEDs being cumulatively brighter than allowed
    _charliplex_list[write_index][CHARLIPLEX_LED_COUNT]=darkness<<8;//put 24-bit darkness value as final element, with 0 LEDs drive (LSB) to indicate it is a simple wait
    _current_list_ptr = _charliplex_list[write_index];
    _charliplex_index = write_index;
    //dma_channel_set_read_addr(_data_chan, _current_list_ptr, true);
}

void Charlieplex::set_brightness(uint8_t index,uint8_t brightness)
{
  _api_brightness[index]=brightness;
}

void Charlieplex::set_max_effective_led_count(uint8_t count)
{//used to calculate the blanking interval at the end of a frame to keep the brightness stable while minimizing flickering
  _max_effective_led_count=count;
}