#include "led.h"

//uint Charlieplex::_sm_offset = -1; //define static (upload pio program only one time)

const AnimationMapping animation_table[] = {
    {"Off",             &Charlieplex::animation_off},
    {"Auto Cycle",      &Charlieplex::animation_cycle},
    {"Blink",           &Charlieplex::animation_blink},
    {"Fire",            &Charlieplex::animation_fire},
    {"Gyroscope",       &Charlieplex::animation_gyroscope},
    //{"Menu Depth",      &Charlieplex::animation_menu_depth},
    {"Microphone",      &Charlieplex::animation_microphone},
    //{"Pulse",           &Charlieplex::animation_pulse},
    {"Rainbow Fade",    &Charlieplex::animation_rainbow_fade},
    {"Stars",           &Charlieplex::animation_stars},
    {"Static Green",    &Charlieplex::animation_static_green},
    {"Static Red",      &Charlieplex::animation_static_red},
    {"Steeple Chase",   &Charlieplex::animation_steeple_chase}
};

Charlieplex::Charlieplex(bool is_upper)
{
  _pio_index=is_upper;
  _first_pin=is_upper?LED_LOWER_START_PIN:LED_UPPER_START_PIN;
  //_current_list_ptr = _charliplex_list[0];
  _current_list_ptr = &_charliplex_list[0][0]; 
}

void Charlieplex::begin(PIOProgramManager &pio_program_manager){
    for(int iter=_first_pin;iter<_first_pin+8;iter++){
      pinMode(iter,INPUT);
      gpio_disable_pulls(iter);
    }//init 8 contiguous LEDs as expected by PIO program

    // 1. Initialize PIO
    //if(_sm_offset == (uint)-1)
    //  _sm_offset = pio_add_program(_pio, &charlieplex_dma_program);//load once in main

    //_sm = pio_claim_unused_sm(_pio, true);//for each charlieplex LED (one for lower, one for upper)
    PIO pio=pio_program_manager.get_pio();
    uint sm=pio_program_manager.allocate_sm();
    uint sm_offset=pio_program_manager.get_offset();
    charlieplex_dma_program_init(pio, sm, sm_offset, _first_pin, 8);

    // 2. Configure DATA DMA (The worker)
    _data_chan = dma_claim_unused_channel(true);
    dma_channel_config c_data = dma_channel_get_default_config(_data_chan);
    channel_config_set_transfer_data_size(&c_data, DMA_SIZE_32);
    channel_config_set_read_increment(&c_data, true);
    channel_config_set_write_increment(&c_data, false);
    channel_config_set_dreq(&c_data, pio_get_dreq(pio, sm, true));
    
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
        &pio->txf[sm],     
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
    /*if (_sm != (uint)-1) {
        pio_sm_set_enabled(_pio, _sm, false);
        
        // Clear the PIO Tx FIFO to drain any lingering matrix transitions
        pio_sm_clear_fifos(_pio, _sm);
        
        // Release the state machine resource back to the Pico SDK pool
        pio_sm_unclaim(_pio, _sm);
        _sm = (uint)-1; // Mark as unallocated
    }*/

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
    for (int iter = _first_pin; iter < _first_pin+8; iter++) {
        gpio_init(iter);
        gpio_set_dir(iter, GPIO_IN);
    }
}

//called to move the data from RAM to the periphreal (LEDs)
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
  count=max(count,1);
  _max_effective_led_count=count;
}


// ---- Application specific animations ----


void Charlieplex::animation_off(SensorSuite &sensor_suite)
{//leds are cleared to 0 on each use, so nothing to set here
    set_max_effective_led_count(1);
    for(int8_t iter = 0; iter < CHARLIPLEX_LED_COUNT; iter++) set_brightness(iter, 0);
}

void Charlieplex::animation_blink(SensorSuite &sensor_suite)
{
    // Configure the speed of the individual LED pulses
    const uint32_t pulse_speed_divisor = 256; 

    // Target a low active count to match your power budget
    set_max_effective_led_count(5); 

    for (uint8_t iter = 0; iter < CHARLIPLEX_LED_COUNT; iter++) {
        // 1. PHASE SHIFTING: Create a unique mathematical seed for this specific LED index.
        // Multiplying by a prime number (like 37) maps different pins to wildly different starting offsets.
        uint32_t led_seed = iter * 37;

        // 2. TIME MAP GENERATION: Calculate a smooth, rolling time value unique to this LED.
        // Adding the seed to millis() offsets each LED's timeline so they don't sync up.
        uint32_t led_time = (millis() / pulse_speed_divisor) + led_seed;

        // 3. STATISTICAL RANDOM THRESHOLD CHECK:
        // We use a pseudo-random hash (multiplying by a large prime, then shifting) to check the state.
        // Modulo 10 creates a 10-step rhythm for each individual LED timeline.
        uint8_t hash_step = ((led_time * 0x45D9F3B) >> 16) % 10;

        // An individual LED turns on ONLY when its specific timeline lands on step 0 or 1.
        // 2 steps out of 10 = ~20% probability. 
        // 20% of 24 LEDs = ~4.8 (roughly 5) LEDs illuminated simultaneously at any given time.
        if (hash_step < 2) {
            set_brightness(iter, 255);
        } else {
            set_brightness(iter, 0);
        }
    }
}

void Charlieplex::animation_cycle(SensorSuite &sensor_suite)
{
    const uint32_t cycle_ms=12000; //how often to change between screen savers
    const uint8_t screen_saver_count=4; //how many screensavers to cycle through
    uint8_t screen_saver_index=millis()/cycle_ms%screen_saver_count;
    switch(screen_saver_index)
    {
        case 0: animation_blink(sensor_suite); return;
        case 1: animation_rainbow_fade(sensor_suite); return;
        case 2: animation_steeple_chase(sensor_suite); return;
        case 3: animation_stars(sensor_suite); return;
        case 4: animation_off(sensor_suite); return;
        case 5: animation_off(sensor_suite); return;
        case 6: animation_off(sensor_suite); return;
        case 7: animation_off(sensor_suite); return;
        case 8: animation_off(sensor_suite); return;
        case 9: animation_off(sensor_suite); return;
        case 10: animation_off(sensor_suite); return;
        case 11: animation_off(sensor_suite); return;
        default: animation_off(sensor_suite); return;
    }
}

void Charlieplex::animation_fire(SensorSuite &sensor_suite)
{
    set_max_effective_led_count(CHARLIPLEX_LED_COUNT/4);

    unsigned long current_time = millis();
    uint8_t half_count = CHARLIPLEX_LED_COUNT / 2;

    for(uint8_t iter=0; iter<half_count; iter++)
    {
        // Normalize position from 0.0 (bottom/near zero) to 1.0 (top/higher index)
        float progress = (float)iter / half_count;

        // Combine multiple sine waves for organic flickering
        float flicker1 = sin(current_time * 0.015f - iter * 2.0f);
        float flicker2 = cos(current_time * 0.007f + iter * 1.5f);
        float flicker3 = sin(current_time * 0.035f - iter * 3.7f);
        
        // Raw flicker value mapped strictly to a 0.0 to 1.0 range
        float total_flicker = ((flicker1 * 0.5f) + (flicker2 * 0.3f) + (flicker3 * 0.2f)) * 0.5f + 0.5f;
        
        // MODERATE CONTRAST: Exponent scales from 1.0 up to 6.0 at the top.
        // This widens the dark gaps noticeably without making sparks completely disappear.
        float flicker_exponent = 1.0f + (progress * 5.0f);
        total_flicker = pow(total_flicker, flicker_exponent);

        // Gentle squared curve for baseline decay.
        // Drops off faster than linear, but keeps the lower-middle indexes feeling warm.
        float base_glow = pow(1.0f - progress, 2.0f) * 0.8f;
        
        // Combine baseline glow with the sharpened flicker peaks
        float final_brightness = (base_glow + total_flicker) * 255.0f;
        
        // Constrain the final value to valid 8-bit bounds
        final_brightness = max(final_brightness, 0.0f);
        final_brightness = min(final_brightness, 255.0f);
        
        // Maintain your color gradient logic
        uint8_t red = (uint8_t)(final_brightness * (CHARLIPLEX_LED_COUNT*.7 - iter) / (CHARLIPLEX_LED_COUNT*.7));
        uint8_t green = (uint8_t)(final_brightness * pow((float)iter,0.7f)*1.5f / half_count);
        green=min(green,red<30?red:red-30);
        
        set_brightness(iter, red);
        set_brightness(iter + half_count, green);
    }
}

void Charlieplex::animation_gyroscope(SensorSuite &sensor_suite)
{
    set_max_effective_led_count(CHARLIPLEX_LED_COUNT/4);

    for(uint8_t iter=0;iter<CHARLIPLEX_LED_COUNT;iter++)
    {
        float brightness;
        float accel=(sensor_suite.imu.get_accel(1))*2.0;//imu.get_accel(1);
        float gyro=sensor_suite.imu.get_gyro(2)/100.0;
        if(iter<CHARLIPLEX_LED_COUNT/2)
        {
          brightness=gyro*.5+.5*accel+sensor_suite.imu.get_accel(1)*.75;
        }else{
          brightness=gyro*.8+.2*accel+sensor_suite.imu.get_accel(1)*.75;
        }
        brightness=max(brightness,-1.0);
        brightness=min(brightness,1.0);
        brightness=(0.25-brightness/4)*CHARLIPLEX_LED_COUNT;
        brightness=255*(1.4-abs(brightness-(iter%(CHARLIPLEX_LED_COUNT/2)))/3.0);
        brightness=max(brightness,0);
        brightness=min(brightness,255);
        uint8_t brightness8=(uint8_t)brightness;
        set_brightness(iter,brightness8);
    }
}

void Charlieplex::animation_menu_depth(SensorSuite &sensor_suite)
{//show how deep the user is in the menu structure
    set_max_effective_led_count(CHARLIPLEX_LED_COUNT/2);
    int8_t menu_depth=sensor_suite.screen_manager.get_screen_stack_depth();
    const uint8_t green=55;
    const uint8_t red=0;
    const uint8_t MAX_MENU_DEPTH=5;
    uint8_t min_led=min(max(0,(uint8_t)(1.0f*(menu_depth-1)*(CHARLIPLEX_LED_COUNT/2)/MAX_MENU_DEPTH)),(CHARLIPLEX_LED_COUNT/2)-1);
    uint8_t max_led=min(max(0,(uint8_t)(1.0f*(menu_depth+0)*(CHARLIPLEX_LED_COUNT/2)/MAX_MENU_DEPTH)),(CHARLIPLEX_LED_COUNT/2)-1);
    for(uint8_t iter=0;iter<CHARLIPLEX_LED_COUNT/2;iter++)
    {
        if(iter>=min_led && iter<max_led)
        {
            set_brightness(iter,red);
            set_brightness(iter+CHARLIPLEX_LED_COUNT/2,green);
        }else{
            set_brightness(iter,0);
            set_brightness(iter+CHARLIPLEX_LED_COUNT/2,0);
        }
    }
}

void Charlieplex::animation_microphone(SensorSuite &sensor_suite)
{
    set_max_effective_led_count(CHARLIPLEX_LED_COUNT/4+1);

    float mean_square=sensor_suite.microphone.get_mean_square();
    float audio_level=min(CHARLIPLEX_LED_COUNT/2,1+(log2f(max(1,mean_square))/10.0)*(CHARLIPLEX_LED_COUNT/2));
    for(uint8_t iter=0;iter<CHARLIPLEX_LED_COUNT/2;iter++)
    {
        uint8_t red=0;
        uint8_t green=0;
        if(iter<audio_level)
        {
            red=(uint8_t)(iter*255/(CHARLIPLEX_LED_COUNT/2));
            green=255-red;
        }
        if(iter==(uint8_t)audio_level)
        {//if the tip of the sound bar, then give it a fractional brightness
            red=(uint8_t)((audio_level-(uint8_t)audio_level)*iter*255/(CHARLIPLEX_LED_COUNT/2));
            green=(uint8_t)((audio_level-(uint8_t)audio_level)*(255-(iter*255/(CHARLIPLEX_LED_COUNT/2))));
        }
        set_brightness(iter,red);
        set_brightness(iter+CHARLIPLEX_LED_COUNT/2,green);
    }
}

void Charlieplex::animation_pulse(SensorSuite &sensor_suite)
{
    set_max_effective_led_count(CHARLIPLEX_LED_COUNT/4+1);

    for(uint8_t iter=0;iter<CHARLIPLEX_LED_COUNT;iter++)
    {
      //slow fade
      uint16_t brightness_upper = (-millis()/8); 
      if(brightness_upper & 0x0100) brightness_upper=255-(uint8_t)brightness_upper;//fade fully off half the time
      if(iter<CHARLIPLEX_LED_COUNT/2) brightness_upper^=0xFF;//invert red out of phase by 180 degrees from green
      set_brightness(iter,(uint8_t)brightness_upper/4);//dim the bulk of the leds
      if((millis()/75)%(CHARLIPLEX_LED_COUNT/2)==iter%(CHARLIPLEX_LED_COUNT/2)) set_brightness(iter,(uint8_t)brightness_upper);//make one brighter
    }
}

void Charlieplex::animation_rainbow_fade(SensorSuite &sensor_suite)
{
    set_max_effective_led_count(CHARLIPLEX_LED_COUNT/2);

    for(uint8_t iter=0;iter<CHARLIPLEX_LED_COUNT;iter++)
    {
      //slow fade
      uint16_t brightness_upper = (-millis()/8)+iter*32; 
      if(brightness_upper & 0x0100) brightness_upper=255-(uint8_t)brightness_upper;//fade fully off half the time
      set_brightness(iter,(uint8_t)brightness_upper);
    }
}

void Charlieplex::animation_stars(SensorSuite &sensor_suite)
{
    set_max_effective_led_count(CHARLIPLEX_LED_COUNT/8);
    const uint16_t period=8500;
    for (uint8_t iter = 0; iter < CHARLIPLEX_LED_COUNT/2; iter++)
    {
        uint32_t phase=(millis()+((iter * 13010771)) * 2654435761U)%period;
        uint32_t color = (((millis() + iter*1000) / period) ^ (iter * 13010771)) * 2654435761U;
        uint8_t red=0;//default off
        uint8_t green=0;
        if(phase<period/4)
        {//getting brighter
            red=  (  color %255)*phase/(period/4);
            green=((-color)%255)*phase/(period/4);
        }else if(phase<period/2){//getting dimmer
            red=  (  color %255)*((period/2)-phase)/(period/4);
            green=((-color)%255)*((period/2)-phase)/(period/4);
        }
        set_brightness(iter,red);
        set_brightness(iter+CHARLIPLEX_LED_COUNT/2,green);
    }
}

void Charlieplex::animation_static_green(SensorSuite &sensor_suite)
{
    set_max_effective_led_count(CHARLIPLEX_LED_COUNT/2);
    for (uint8_t iter = CHARLIPLEX_LED_COUNT/2; iter < CHARLIPLEX_LED_COUNT; iter++) set_brightness(iter,255);
}
void Charlieplex::animation_static_red(SensorSuite &sensor_suite)
{
    set_max_effective_led_count(CHARLIPLEX_LED_COUNT/2);
    for (uint8_t iter = 0; iter < CHARLIPLEX_LED_COUNT/2; iter++) set_brightness(iter,255);
}
void Charlieplex::animation_steeple_chase(SensorSuite &sensor_suite)
{
    set_max_effective_led_count(CHARLIPLEX_LED_COUNT/4+1);

    for(uint8_t iter=0;iter<CHARLIPLEX_LED_COUNT;iter++)
    {
      //slow fade
      uint16_t brightness_upper = (-millis()/8); 
      if(brightness_upper & 0x0100) brightness_upper=255-(uint8_t)brightness_upper;//fade fully off half the time
      if(iter<CHARLIPLEX_LED_COUNT/2) brightness_upper^=0xFF;//invert red out of phase by 180 degrees from green
      set_brightness(iter,(uint8_t)brightness_upper/4);//dim the bulk of the leds
      if((millis()/75)%7==iter%(CHARLIPLEX_LED_COUNT/2)%7) set_brightness(iter,(uint8_t)brightness_upper);//make one brighter
    }
}

// given a string "name" of the animation to play, update the dest_func with a pointer to the above application-specific animation to play in response
//Note: also need to update menu options in graphics.ino to make new options visible to the user
/*bool Charlieplex::get_animation_by_name(const char * name, AnimationFunc &dest_func) {
    if (!name) return false;

    if (strcmp(name, "Off") == 0) {
        dest_func = &Charlieplex::animation_off;
        return true;
    }
    else if (strcmp(name, "Auto Cycle") == 0) {
        dest_func = &Charlieplex::animation_cycle;
        return true;
    }
    else if (strcmp(name, "Blink") == 0) {
        dest_func = &Charlieplex::animation_blink;
        return true;
    }
    else if (strcmp(name, "Fire") == 0) {
        dest_func = &Charlieplex::animation_fire;
        return true;
    }
    else if (strcmp(name, "Gyroscope") == 0) {
        dest_func = &Charlieplex::animation_gyroscope;
        return true;
    }
    else if (strcmp(name, "Microphone") == 0) {
        dest_func = &Charlieplex::animation_microphone;
        return true;
    }
    else if (strcmp(name, "Pulse") == 0) {
        dest_func = &Charlieplex::animation_pulse;
        return true;
    }
    else if (strcmp(name, "Rainbow Fade") == 0) {
        dest_func = &Charlieplex::animation_rainbow_fade;
        return true;
    }
    else if (strcmp(name, "Stars") == 0) {
        dest_func = &Charlieplex::animation_stars;
        return true;
    }
    else if (strcmp(name, "Static Green") == 0) {
        dest_func = &Charlieplex::animation_static_green;
        return true;
    }
    else if (strcmp(name, "Static Red") == 0) {
        dest_func = &Charlieplex::animation_static_red;
        return true;
    }
    else if (strcmp(name, "Steeple Chase") == 0) {
        dest_func = &Charlieplex::animation_steeple_chase;
        return true;
    }

    // String not recognized by this class instance
    return false; 
}*/

bool Charlieplex::get_animation_by_name(const std::string name, AnimationFunc &dest_func) {
    if (name.empty()) return false;

    size_t count = get_animation_count();
    for (size_t i = 0; i < count; i++) {
        if (name==animation_table[i].name) {
            dest_func = animation_table[i].func;
            return true;
        }
    }
    return false;
}

uint8_t Charlieplex::get_animation_count() { return sizeof(animation_table) / sizeof(animation_table[0]); }
std::string Charlieplex::get_animation_at(uint8_t index){ if (index >= get_animation_count()) return "Unknown"; return animation_table[index].name; }