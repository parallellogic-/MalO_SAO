#include "graphics.h"

Graphics::Graphics()
{

}

void Graphics::display_flush_cb(lv_display_t * disp, const lv_area_t * area, uint8_t * px_map) {

    Graphics* instance = (Graphics*)lv_display_get_user_data(disp);
    if (instance && instance->_sensor_suite) {
      instance->lvgl2spi((uint8_t*)px_map,instance->_sensor_suite->screen);
    }
    lv_display_flush_ready(disp);
}

// --- Custom LVGL Input Device Driver ---
void Graphics::button_read_cb(lv_indev_t * indev, lv_indev_data_t * data) {
  Graphics* instance = (Graphics*)lv_indev_get_user_data(indev);
  if (!instance || !instance->_sensor_suite) return;

  uint8_t down_button=instance->_sensor_suite->touch.get_down_button();
  //Serial.printf("\nButton: %d\n\n",down_button);
    if (down_button==1) {
        data->key = LV_KEY_NEXT;
        data->state = LV_INDEV_STATE_PRESSED;
    } else if (down_button==2) {
        data->key = LV_KEY_ESC;
        data->state = LV_INDEV_STATE_PRESSED;
    } else if (down_button==3) {
        data->key = LV_KEY_ESC;
        data->state = LV_INDEV_STATE_PRESSED;
    } else if (down_button==4) {
        data->key = LV_KEY_ENTER;
        data->state = LV_INDEV_STATE_PRESSED;
    } else if (down_button==5) {
        data->key = LV_KEY_PREV;
        data->state = LV_INDEV_STATE_PRESSED;
    } else if (down_button==6) {
        data->key = LV_KEY_UP;
        data->state = LV_INDEV_STATE_PRESSED;
    } else if (down_button==7) {
        data->key = LV_KEY_NEXT;
        data->state = LV_INDEV_STATE_PRESSED;
    } else if (down_button==8) {
        data->key = LV_KEY_LEFT;
        data->state = LV_INDEV_STATE_PRESSED;
    } else if (down_button==9) {
        data->key = LV_KEY_DOWN;
        data->state = LV_INDEV_STATE_PRESSED;
    } else if (down_button==10) {
        data->key = LV_KEY_RIGHT;
        data->state = LV_INDEV_STATE_PRESSED;
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

// --- Menu Event Handler ---
void Graphics::menu_event_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * obj = (lv_obj_t*)lv_event_get_target(e); // Cast added here!

    if (code == LV_EVENT_CLICKED) {
        // Extract the 'this' Graphics instance from the active menu list object
        //Graphics* instance = (Graphics*)lv_obj_get_user_data(obj);
        lv_obj_t * parent_list = lv_obj_get_parent(obj);
        Graphics* instance = (Graphics*)lv_obj_get_user_data(parent_list);
        if (!instance) return;

        // Safe usage with LVGL v9 string-fetching methods
//        const char * text = lv_list_get_button_text(instance->_menu_list, obj);
//        Serial.printf("Menu Option Clicked: %s\n", text);

        // Use 'instance->' to route to your state machines
        // instance->initialize_level_subsystem(1);
    }
}

void Graphics::begin(SensorSuite &sensor_suite)
{
  _sensor_suite=&sensor_suite;

    lv_init();

    // Create and configure the LVGL display
    lv_display_t * disp = lv_display_create(SCREEN_WIDTH_PX, SCREEN_HEIGHT_PX);
    lv_display_set_buffers(disp, _canvas_buffer, NULL, sizeof(_canvas_buffer), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_user_data(disp, this); 
    lv_display_set_flush_cb(disp, display_flush_cb);
    lv_obj_set_style_bg_color(lv_screen_active(), lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(lv_screen_active(), LV_OPA_COVER, LV_PART_MAIN);

    // Create and configure the LVGL keyboard/button input device
    lv_indev_t * indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_KEYPAD);
    lv_indev_set_user_data(indev, this);
    lv_indev_set_read_cb(indev, button_read_cb);

    // Create an input group for keyboard navigation
    lv_group_t * g = lv_group_create();
    lv_group_set_default(g);
    lv_indev_set_group(indev, g);

    // -- debug application specific parts --


    static lv_style_t style_menu_item;
    lv_style_init(&style_menu_item);
    lv_style_set_bg_color(&style_menu_item, lv_color_black());
    lv_style_set_text_color(&style_menu_item, lv_color_white());
    lv_style_set_border_width(&style_menu_item, 0);
    lv_style_set_pad_all(&style_menu_item, 0);

    // --- UI Layout: Three-Option Menu ---
    _menu_list = lv_list_create(lv_screen_active());
    lv_obj_set_size(_menu_list, 128, 128);
    lv_obj_add_style(_menu_list, &style_menu_item, LV_PART_MAIN); // Reused pointer!
//    lv_obj_center(_menu_list);

    // FIX 2: Turn the list container body completely black and strip default styling
    //lv_obj_set_style_bg_color(_menu_list, lv_color_black(), LV_PART_MAIN);
    //lv_obj_set_style_border_width(_menu_list, 0, LV_PART_MAIN);      // Remove default border
    //lv_obj_set_style_pad_all(_menu_list, 0, LV_PART_MAIN);          // Remove padding gaps
    //lv_obj_set_style_shadow_width(_menu_list, 0, LV_PART_MAIN);     // Strip hidden shadow blooms

    // Add list items and associate them with the event handler
    lv_obj_t * btn1 = lv_list_add_button(_menu_list, NULL, "Animations");
    lv_obj_add_event_cb(btn1, menu_event_cb, LV_EVENT_CLICKED, NULL);
    //lv_obj_set_user_data(btn1, this); // Binds "this" instance for menu clicks
    lv_group_add_obj(g, btn1); // Add to navigation group
    lv_obj_add_style(btn1, &style_menu_item, LV_PART_MAIN);

    lv_obj_t * btn2 = lv_list_add_button(_menu_list, NULL, "Messages");
    lv_obj_add_event_cb(btn2, menu_event_cb, LV_EVENT_CLICKED, NULL);
    //lv_obj_set_user_data(btn2, this); // Binds "this" instance for menu clicks
    lv_group_add_obj(g, btn2);
    lv_obj_add_style(btn2, &style_menu_item, LV_PART_MAIN);

    lv_obj_t * btn3 = lv_list_add_button(_menu_list, NULL, "Settings");
    lv_obj_add_event_cb(btn3, menu_event_cb, LV_EVENT_CLICKED, NULL);
    //lv_obj_set_user_data(btn3, this); // Binds "this" instance for menu clicks
    lv_group_add_obj(g, btn3);
    lv_obj_add_style(btn3, &style_menu_item, LV_PART_MAIN);

    /*uint32_t child_count = lv_obj_get_child_count(_menu_list);
    for(uint32_t i = 0; i < child_count; i++) {
        lv_obj_t * child = lv_obj_get_child(_menu_list, i);
        
        // 1. Set Button Background to Black (Default state)
        lv_obj_set_style_bg_color(child, lv_color_black(), LV_STATE_DEFAULT); //sets inactive buttons to black background
        
        // 2. Set Text & Symbol colors to White (Default state)
        lv_obj_set_style_text_color(child, lv_color_white(), LV_STATE_DEFAULT);
        //lv_obj_set_style_image_recolor(child, lv_color_black(), LV_STATE_DEFAULT); // Icons/Symbols
        //lv_obj_set_style_image_recolor_opa(child, LV_OPA_COVER, LV_STATE_DEFAULT);

        // 3. Define Focus State: What happens when the button is highlighted/focused?
        // Let's make the focused item invert back (White background, Black text) so it stands out.
        //lv_obj_set_style_bg_color(child, lv_color_black(), LV_STATE_FOCUS_KEY);
        //lv_obj_set_style_text_color(child, lv_color_white(), LV_STATE_FOCUS_KEY);
        //lv_obj_set_style_image_recolor(child, lv_color_black(), LV_STATE_FOCUS_KEY);
        
        // Ensure standard keypad focus matches the focus style rules
        //lv_obj_set_style_bg_color(child, lv_color_black(), LV_STATE_FOCUSED);
        //lv_obj_set_style_text_color(child, lv_color_white(), LV_STATE_FOCUSED);
        //lv_obj_set_style_image_recolor(child, lv_color_black(), LV_STATE_FOCUSED);
    }*/
}


void Graphics::update()
{
    if(_last_update_ms==0) _last_update_ms=millis();//bootup
    uint32_t current_time_ms=millis();
    lv_tick_inc(current_time_ms-_last_update_ms);
    _last_update_ms=current_time_ms;

    lv_timer_handler();
    //memset(_canvas_buffer, 0x66, sizeof(_canvas_buffer)); //<200 us
    //lvgl2spi(_canvas_buffer,_sensor_suite->screen);


    lv_mem_monitor_t mon;
    lv_mem_monitor(&mon);

    Serial.printf("--- LVGL INTERNAL POOL STATUS ---\n");
    Serial.printf("Total Pool Size: %d bytes\n", mon.total_size);
    Serial.printf("Free Memory Left: %d bytes\n", mon.free_size);
    Serial.printf("Memory Used: %d%% (%d bytes)\n", mon.used_pct, mon.total_size - mon.free_size);
    Serial.printf("Max Memory Ever Used: %d bytes\n", mon.max_used);
    Serial.printf("Memory Fragmentation: %d%%\n", mon.frag_pct);

    if (mon.free_size < 2048) {
        Serial.printf("WARNING: Dangerously low on LVGL memory!\n");
    }
}

void Graphics::end()
{

}

// -- helper methods --


// Custom function to process the canvas buffer, pack upper nibbles, and transmit
//850 us
void Graphics::lvgl2spi(uint8_t* src,Screen &screen) {
    uint32_t packed_idx = 0;
    
    uint8_t* tx_buffer=screen.get_frame_buffer();
    for (int32_t y = 0; y < SCREEN_HEIGHT_PX; y++) {
        for (int32_t x = 0; x < SCREEN_WIDTH_PX; x += 2) {
            
            // --- 90-DEGREE CCW COORDINATE TRANSLATION ---
            // Formula for 90 CCW: New_X = Old_Y, New_Y = (Width - 1) - Old_X
            
            // Calculate source coordinates for the Left output pixel (at column x)
            int32_t src_x_left = (SCREEN_WIDTH_PX - 1) - y;
            int32_t src_y_left = x;
            uint32_t pixel_left_idx = (src_y_left * SCREEN_WIDTH_PX) + src_x_left;

            // Calculate source coordinates for the Right output pixel (at column x + 1)
            int32_t src_x_right = (SCREEN_WIDTH_PX - 1) - y;
            int32_t src_y_right = x + 1;
            uint32_t pixel_right_idx = (src_y_right * SCREEN_WIDTH_PX) + src_x_right;
            //uint32_t pixel_right_idx = pixel_left_idx+SCREEN_WIDTH_PX;

            // Extract the high-frequency luminosity bits (upper nibbles)
            uint8_t left_nibble  = src[pixel_left_idx]  & 0xF0;//_canvas_buffer[pixel_left_idx]  & 0xF0;
            uint8_t right_nibble = src[pixel_right_idx] & 0xF0;//_canvas_buffer[pixel_right_idx] & 0xF0;

            // Pack them perfectly: Left pixel high bits, Right pixel low bits
            tx_buffer[packed_idx++] = left_nibble | (right_nibble >> 4);
        }
    }
    
    // Transmit the fully optimized 4bpp block directly to your display controller
    screen.flush();
}
