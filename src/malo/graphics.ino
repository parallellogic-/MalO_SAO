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
    lv_obj_t * obj = (lv_obj_t*)lv_event_get_target(e); 

    if (code == LV_EVENT_CLICKED) {
        // Walk up to the main container to grab the 'this' context pointer
        lv_obj_t * menu_container = lv_obj_get_parent(obj);
        Graphics* instance = (Graphics*)lv_obj_get_user_data(menu_container);
        if (!instance) return;

        // Fetch text directly from the label widget safely
        const char * text = lv_label_get_text(obj);
        // Serial.printf("Label Menu Option Clicked: %s\n", text);

        // Map text selection to your application states
        // if(strcmp(text, "Animations") == 0) instance->run_animations();
    }
}

void Graphics::begin(SensorSuite &sensor_suite)
{
    _sensor_suite = &sensor_suite;
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

    lv_group_t * g = lv_group_create();
    lv_group_set_default(g);
    lv_indev_set_group(indev, g);

    // ==========================================
    // FIXED CORNERSTONE STATIC STYLE CONTAINERS
    // ==========================================
    static lv_style_t style_menu_item_main;
    lv_style_init(&style_menu_item_main);
    lv_style_set_bg_opa(&style_menu_item_main, LV_OPA_TRANSP); // No state parameter here
    lv_style_set_text_color(&style_menu_item_main, lv_color_white());
    lv_style_set_pad_ver(&style_menu_item_main, 4);
    lv_style_set_pad_hor(&style_menu_item_main, 6);
    
    static lv_style_t style_menu_item_focused;
    lv_style_init(&style_menu_item_focused);
    lv_style_set_bg_opa(&style_menu_item_focused, LV_OPA_COVER);
    lv_style_set_bg_color(&style_menu_item_focused, lv_color_white());
    lv_style_set_text_color(&style_menu_item_focused, lv_color_black());

    // ==========================================
    // ULTRA-LIGHT OBJECT CREATION
    // ==========================================
    _menu_list = lv_obj_create(lv_screen_active());
    lv_obj_set_user_data(_menu_list, this); 
    lv_obj_set_size(_menu_list, 128, 128);
    lv_obj_set_flex_flow(_menu_list, LV_FLEX_FLOW_COLUMN);
    
    lv_obj_set_style_bg_color(_menu_list, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_border_width(_menu_list, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(_menu_list, 0, LV_PART_MAIN);

    // --- Create Menu Item 1 ---
    lv_obj_t * lbl1 = lv_label_create(_menu_list);
    lv_label_set_text(lbl1, "Animations");
    lv_obj_set_width(lbl1, LV_PCT(100));
    
    // Fixed type safety compilation error: Add flags independently
    lv_obj_add_flag(lbl1, LV_OBJ_FLAG_CLICKABLE); 
    lv_obj_add_flag(lbl1, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
    
    // Bind main style for default view, and bind focus style exclusively for LV_STATE_FOCUSED
    lv_obj_add_style(lbl1, &style_menu_item_main, LV_STATE_DEFAULT);
    lv_obj_add_style(lbl1, &style_menu_item_focused, LV_STATE_FOCUSED);
    
    lv_obj_add_event_cb(lbl1, menu_event_cb, LV_EVENT_CLICKED, NULL);
    lv_group_add_obj(g, lbl1);

    // --- Create Menu Item 2 ---
    lv_obj_t * lbl2 = lv_label_create(_menu_list);
    lv_label_set_text(lbl2, "Messages");
    lv_obj_set_width(lbl2, LV_PCT(100));
    lv_obj_add_flag(lbl2, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(lbl2, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
    lv_obj_add_style(lbl2, &style_menu_item_main, LV_STATE_DEFAULT);
    lv_obj_add_style(lbl2, &style_menu_item_focused, LV_STATE_FOCUSED);
    lv_obj_add_event_cb(lbl2, menu_event_cb, LV_EVENT_CLICKED, NULL);
    lv_group_add_obj(g, lbl2);

    // --- Create Menu Item 3 ---
    lv_obj_t * lbl3 = lv_label_create(_menu_list);
    lv_label_set_text(lbl3, "Settings");
    lv_obj_set_width(lbl3, LV_PCT(100));
    lv_obj_add_flag(lbl3, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(lbl3, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
    lv_obj_add_style(lbl3, &style_menu_item_main, LV_STATE_DEFAULT);
    lv_obj_add_style(lbl3, &style_menu_item_focused, LV_STATE_FOCUSED);
    lv_obj_add_event_cb(lbl3, menu_event_cb, LV_EVENT_CLICKED, NULL);
    lv_group_add_obj(g, lbl3);
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
