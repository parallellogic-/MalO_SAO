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

// --- Shared Menu Focus Monitor ---
void Graphics::menu_focus_cb(lv_event_t * e) {
    lv_obj_t * child = (lv_obj_t*)lv_event_get_target(e);
    lv_obj_t * parent_list = lv_obj_get_parent(child);
    
    // MEMORY FEATURE: Save this item pointer as the last focused element inside the parent container!
    // This uses zero dynamic heap memory allocations.
    lv_obj_set_user_data(parent_list, child);

    // Calculate centering scroll offset exactly like before
    int32_t item_y = lv_obj_get_y(child);
    int32_t item_height = lv_obj_get_height(child);
    int32_t container_height = lv_obj_get_height(parent_list);
    int32_t target_scroll_y = item_y + (item_height / 2) - (container_height / 2);

    lv_obj_scroll_to_y(parent_list, target_scroll_y, LV_ANIM_ON);
}

// --- Menu Interaction & Switch Routine ---
void Graphics::switch_menu(lv_obj_t * new_menu, bool remember_last_selection) {
    Graphics* instance = (Graphics*)lv_display_get_user_data(NULL); // Fallback safe grab
    if (!new_menu) return;

    // 1. Hide the old active menu layout panel if it exists
    if (instance->_active_menu) {
        lv_obj_add_flag(instance->_active_menu, LV_OBJ_FLAG_HIDDEN);
    }

    // 2. Erase all old button items from the keypad navigation ring
    lv_group_remove_all_objs(instance->_input_group);

    // 3. Make the target submenu container visible on screen
    lv_obj_remove_flag(new_menu, LV_OBJ_FLAG_HIDDEN);
    instance->_active_menu = new_menu;

    // 4. Reload the keypad group ring with the child items inside the new menu panel
    uint32_t child_count = lv_obj_get_child_count(new_menu);
    for (uint32_t i = 0; i < child_count; i++) {
        lv_obj_t * child = lv_obj_get_child(new_menu, i);
        // Only insert clickable menu nodes into our keypad loop
        if (lv_obj_has_flag(child, LV_OBJ_FLAG_CLICKABLE)) {
            lv_group_add_obj(instance->_input_group, child);
        }
    }

    // 5. RESTORE OR RESET MEMORY: Determine which item should receive initial focus highlight
    lv_obj_t * target_focus_node = NULL;
    
    if (remember_last_selection) {
        // Look up the historical child node stored inside this container's user data
        target_focus_node = (lv_obj_t*)lv_obj_get_user_data(new_menu);
    }

    // If no memory existed or remember flag is off, fall back to focusing the first child item
    if (!target_focus_node && child_count > 0) {
        target_focus_node = lv_obj_get_child(new_menu, 0);
    }

    // Force the keypad focus subsystem onto the calculated node
    if (target_focus_node) {
        lv_group_focus_obj(target_focus_node);
        // Instantly pop the view straight to it without scrolling lagging behind
        lv_obj_scroll_to_view(target_focus_node, LV_ANIM_OFF);
    }
}

// --- Menu Event Handler ---
void Graphics::menu_event_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * obj = (lv_obj_t*)lv_event_get_target(e); 

    // ==========================================
    // 1. DYNAMIC NAVIGATION TRAVERSAL ENGINE
    // ==========================================
    Graphics* instance = (Graphics*)lv_display_get_user_data(NULL);
    if (!instance) return;
    if (code == LV_EVENT_KEY) {
        uint32_t key = lv_event_get_key(e);

        if (key == LV_KEY_ESC) {
            // Fetch our embedded back-link hidden inside this label's user data
            lv_obj_t * parent_menu = (lv_obj_t*)lv_obj_get_user_data(obj);
            
            // If a link exists, seamlessly back up exactly 1 level deep, preserving history
            if (parent_menu) {
                instance->switch_menu(parent_menu, true);
                instance->led_cb(true); //turn off leds if leaving the Animations menu
            }
            return;
        }
        
        if (key == LV_KEY_HOME) {
            // Direct escape straight to the root menu frame from any depth layer
            if (instance->_active_menu != instance->_menu_main) {
                instance->switch_menu(instance->_menu_main, true);
                instance->led_cb(true); //turn off leds if leaving the Animations menu
            }
            return;
        }
    }

    // ==========================================
    // 2. ITEM CLICK MANAGEMENT (FORWARDS)
    // ==========================================
    if (code == LV_EVENT_CLICKED) {
        const char * text = lv_label_get_text(obj);

        lv_obj_t * parent_panel = lv_obj_get_parent(obj);
        if (!parent_panel) return;

        if (instance->_active_menu != nullptr && instance->_active_menu == instance->_menu_animations_upper_leds) {
            if (instance->_sensor_suite->led_upper.get_animation_by_name(text, instance->_active_animation_upper)) return;
        }
        if (instance->_active_menu != nullptr && instance->_active_menu == instance->_menu_animations_lower_leds) {
            if (instance->_sensor_suite->led_lower.get_animation_by_name(text, instance->_active_animation_lower)) return;
        }

        if(parent_panel==instance->_menu_animations_screen && strcmp(text, "Off") == 0)
        {//request to blank the display
            instance->_is_in_level = true;
            
            // Hide the active menu interface 
            if (instance->_active_menu) {
                lv_obj_add_flag(instance->_active_menu, LV_OBJ_FLAG_HIDDEN);
            }
            
            // Unhide the raw canvas object interface wrapper
            lv_obj_remove_flag(instance->_level_canvas, LV_OBJ_FLAG_HIDDEN);
            
            // Clear or seed the screen array before rendering begins
            memset(instance->_level_buffer, 0, sizeof(instance->_level_buffer));
            return; 
        }
        //--- Deep Tree Forward Routers ---
        // Level 1 -> Level 2
        if (strcmp(text, "Settings") == 0) {
            instance->switch_menu(instance->_menu_settings, true);
        }
        else if (strcmp(text, "Animations") == 0) {
            instance->switch_menu(instance->_menu_animations, false);
        }
        // Level 2 -> Level 3 (Deep nested leaf node branch)
        else if (strcmp(text, "Upper LEDs") == 0) {
            instance->switch_menu(instance->_menu_animations_upper_leds, false);
        }
        else if (strcmp(text, "Lower LEDs") == 0) {
            instance->switch_menu(instance->_menu_animations_lower_leds, false);
        }
        else if (strcmp(text, "Screen") == 0) {
            instance->switch_menu(instance->_menu_animations_screen, false);
        }
        else if (strcmp(text, "Levels") == 0) {
            instance->switch_menu(instance->_menu_levels, false);
        }
        else if (strcmp(text, "Messages") == 0) {
            instance->switch_menu(instance->_menu_messages, false);
        }
        else if (strcmp(text, "Periphreal Test") == 0) {
            instance->switch_menu(instance->_menu_periphreal_test, false);
        }
        // Unified Back string tracker handles older menu formats seamlessly
        else if (strcmp(text, "Back") == 0) {
            lv_obj_t * parent_menu = (lv_obj_t*)lv_obj_get_user_data(obj);
            if (parent_menu) instance->switch_menu(parent_menu, true);
        }
    }
    instance->led_cb(true); //turn off leds if leaving the Animations menu
}

void Graphics::button_read_cb(lv_indev_t * indev, lv_indev_data_t * data) {
    Graphics* instance = (Graphics*)lv_indev_get_user_data(indev);
    if (!instance || !instance->_sensor_suite) return;

    uint8_t current_button = instance->_sensor_suite->touch.get_down_button();

    if (current_button == 0) {
        instance->_last_raw_button = 0; 
        data->state = LV_INDEV_STATE_RELEASED;
        return;
    }

    data->state = LV_INDEV_STATE_PRESSED;

    // Prevent autofire repeat streams from breaking menu position transitions
    if (current_button == instance->_last_raw_button) {
        return; 
    }
    instance->_last_raw_button = current_button;

    switch (current_button) {
        case 1:  data->key = LV_KEY_NEXT;  break;
        case 2:  data->key = LV_KEY_HOME;  break;
        case 3:  data->key = LV_KEY_ESC;   break;
        case 4:  data->key = LV_KEY_ENTER; break;
        case 5:  data->key = LV_KEY_ESC;   break;
        case 6:  data->key = LV_KEY_PREV;  break; 
        case 7:  data->key = LV_KEY_ENTER; break;
        case 8:  data->key = LV_KEY_LEFT;   break;
        case 9:  data->key = LV_KEY_NEXT;  break; 
        case 10: data->key = LV_KEY_RIGHT; break;
        default: break;
    }
    lv_obj_invalidate(lv_screen_active());//work-around for sticky menu that shows selected option at the top of the screen instead of the middle where it should be
}

void Graphics::led_cb(bool is_menu_event)
{
    bool is_menu_valid=_active_menu != nullptr;
    bool is_visible_menu=_active_menu == _menu_animations || 
            _active_menu == _menu_animations_upper_leds || 
            _active_menu == _menu_animations_lower_leds || 
            _active_menu == _menu_animations_screen;
    if(!is_menu_event && is_menu_valid && is_visible_menu)
    {//run the update on the currently commanded led animation
        if(_active_animation_lower) (_sensor_suite->led_lower.*_active_animation_lower)(*_sensor_suite);
        if(_active_animation_upper) (_sensor_suite->led_upper.*_active_animation_upper)(*_sensor_suite);
    }
    if(is_menu_event && is_menu_valid && !is_visible_menu)
    {//turn off leds when leaving animations menu
        _sensor_suite->led_lower.animation_off(sensor_suite);
        _sensor_suite->led_upper.animation_off(sensor_suite);
        /*if(is_menu_event)
        {
            Serial.println("\n\nHALT\n\n");
            //while(1);
        }*/
    }
}

void Graphics::begin(SensorSuite &sensor_suite)
{
    _sensor_suite = &sensor_suite;
    lv_init();

    // Configure Display & Input Drivers (Keep your standard code here...)
    lv_display_t * disp = lv_display_create(SCREEN_WIDTH_PX, SCREEN_HEIGHT_PX);
    lv_display_set_buffers(disp, _canvas_buffer, NULL, sizeof(_canvas_buffer), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_user_data(disp, this); 
    lv_display_set_flush_cb(disp, display_flush_cb);


    _level_canvas = lv_canvas_create(lv_screen_active());
    // Bind your private game_frame_buffer array as the canvas asset data target
    // Assumes your display driver defaults to standard RGB565 depth
    lv_canvas_set_buffer(_level_canvas, _level_buffer, SCREEN_WIDTH_PX, SCREEN_HEIGHT_PX, LV_COLOR_FORMAT_L8);
    lv_obj_align(_level_canvas, LV_ALIGN_CENTER, 0, 0);
    // Keep it hidden initially so it doesn't mask out the main selection tree
    lv_obj_add_flag(_level_canvas, LV_OBJ_FLAG_HIDDEN); 
    
    // Create and configure the LVGL keyboard/button input device
    lv_indev_t * indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_KEYPAD);

    // 1. Pass the Graphics class instance pointer to user_data so the callback can access it
    lv_indev_set_user_data(indev, this); 

    // 2. CRITICAL BINDING CALL: Attach your custom edge-detection button reader function
    lv_indev_set_read_cb(indev, button_read_cb);

    // ... input group bindings ...
    lv_obj_set_style_bg_color(lv_screen_active(), lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(lv_screen_active(), LV_OPA_COVER, LV_PART_MAIN);

    _input_group = lv_group_create();
    lv_group_set_default(_input_group);
    lv_indev_set_group(indev, _input_group); // Must be active!

    // Baseline Style Containers
    static lv_style_t style_menu_item_main;
    lv_style_init(&style_menu_item_main);
    lv_style_set_bg_opa(&style_menu_item_main, LV_OPA_TRANSP); 
    lv_style_set_text_color(&style_menu_item_main, lv_color_white());
    lv_style_set_pad_ver(&style_menu_item_main, 6);
    lv_style_set_pad_hor(&style_menu_item_main, 6);
    
    static lv_style_t style_menu_item_focused;
    lv_style_init(&style_menu_item_focused);
    lv_style_set_bg_opa(&style_menu_item_focused, LV_OPA_COVER);
    lv_style_set_bg_color(&style_menu_item_focused, lv_color_white());
    lv_style_set_text_color(&style_menu_item_focused, lv_color_black());
    lv_style_set_radius(&style_menu_item_focused, 4); 

    // ==========================================
    // INITIALIZE ALL SUBMENU CONTAINER FRAMES
    // ==========================================
    // FIXED: Removed all bitwise OR operators ('|') inside the macro code
    // to strictly preserve C++ enum type-safety conversion rules.
    #define PREPARE_MENU_PANEL(panel_var) \
        panel_var = lv_obj_create(lv_screen_active()); \
        lv_obj_set_size(panel_var, 128, 128); \
        lv_obj_set_flex_flow(panel_var, LV_FLEX_FLOW_COLUMN); \
        \
        lv_obj_add_flag(panel_var, LV_OBJ_FLAG_OVERFLOW_VISIBLE); \
        lv_obj_add_flag(panel_var, LV_OBJ_FLAG_SCROLLABLE); \
        \
        lv_obj_remove_flag(panel_var, LV_OBJ_FLAG_SCROLL_ELASTIC); \
        lv_obj_remove_flag(panel_var, LV_OBJ_FLAG_SCROLL_MOMENTUM); \
        \
        lv_obj_set_scrollbar_mode(panel_var, LV_SCROLLBAR_MODE_OFF); \
        lv_obj_set_style_bg_color(panel_var, lv_color_black(), LV_PART_MAIN); \
        lv_obj_set_style_border_width(panel_var, 0, LV_PART_MAIN); \
        lv_obj_set_style_pad_all(panel_var, 0, LV_PART_MAIN); \
        lv_obj_add_flag(panel_var, LV_OBJ_FLAG_HIDDEN);

    // Unpack macro instances cleanly across your private header variables
    PREPARE_MENU_PANEL(_menu_main);
    PREPARE_MENU_PANEL(_menu_animations);
    PREPARE_MENU_PANEL(_menu_animations_upper_leds);
    PREPARE_MENU_PANEL(_menu_animations_lower_leds);
    PREPARE_MENU_PANEL(_menu_animations_screen);
    PREPARE_MENU_PANEL(_menu_settings);
    PREPARE_MENU_PANEL(_menu_levels);
    PREPARE_MENU_PANEL(_menu_messages);
    PREPARE_MENU_PANEL(_menu_periphreal_test);
    #undef PREPARE_MENU_PANEL


    // =================================================================
    // NEW STRUCTURAL ITEM MACRO: LINKS BACKWARD TARGET ON INITIALIZATION
    // =================================================================
    #define ADD_LINKED_ITEM(target_panel, text_str, escape_dest_panel) { \
        lv_obj_t * lbl = lv_label_create(target_panel); \
        lv_label_set_text(lbl, text_str); \
        lv_obj_set_width(lbl, LV_PCT(100)); \
        lv_obj_add_flag(lbl, LV_OBJ_FLAG_CLICKABLE); \
        lv_obj_add_style(lbl, &style_menu_item_main, LV_STATE_DEFAULT); \
        lv_obj_add_style(lbl, &style_menu_item_focused, LV_STATE_FOCUSED); \
        lv_obj_add_event_cb(lbl, menu_focus_cb, LV_EVENT_FOCUSED, NULL); \
        lv_obj_add_event_cb(lbl, menu_event_cb, LV_EVENT_ALL, NULL); \
        \
        /* LINK TRICK: Embed the backwards menu pointer right inside the item! */ \
        lv_obj_set_user_data(lbl, escape_dest_panel); \
    }

    // --- LEVEL 1 (ROOT MAIN MENU) ---
    // Roots pass NULL because pressing ESC at the base level shouldn't go anywhere backwards
    ADD_LINKED_ITEM(_menu_main, "Animations",   NULL);
    ADD_LINKED_ITEM(_menu_main, "Levels",       NULL);
    ADD_LINKED_ITEM(_menu_main, "Messages",     NULL);
    ADD_LINKED_ITEM(_menu_main, "Achievements", NULL);
    ADD_LINKED_ITEM(_menu_main, "Settings",     NULL);

    // --- LEVEL 2 (SUBMENUS) ---
    // Items inside these submenus will link back to the Root Main Menu on ESC
    ADD_LINKED_ITEM(_menu_levels, "Tic Tac Toe",      _menu_main);
    ADD_LINKED_ITEM(_menu_levels, "Pong",             _menu_main);
    ADD_LINKED_ITEM(_menu_levels, "Box",              _menu_main);
    ADD_LINKED_ITEM(_menu_levels, "Snake",            _menu_main);
    ADD_LINKED_ITEM(_menu_levels, "Platformer",       _menu_main);
    ADD_LINKED_ITEM(_menu_levels, "Back",             _menu_main);

    ADD_LINKED_ITEM(_menu_settings, "Alert Type",     _menu_main);
    ADD_LINKED_ITEM(_menu_settings, "LED Brightness", _menu_main);
    ADD_LINKED_ITEM(_menu_settings, "Periphreal Test",_menu_main);
    ADD_LINKED_ITEM(_menu_settings, "IR Transmit Rate",_menu_main); //bit clock rate
    ADD_LINKED_ITEM(_menu_settings, "Mount USB",      _menu_main);
    ADD_LINKED_ITEM(_menu_settings, "Back",           _menu_main);

    ADD_LINKED_ITEM(_menu_animations, "Upper LEDs",   _menu_main);
    ADD_LINKED_ITEM(_menu_animations, "Lower LEDs",   _menu_main);
    ADD_LINKED_ITEM(_menu_animations, "Screen",       _menu_main);
    ADD_LINKED_ITEM(_menu_animations, "Back",         _menu_main);

    ADD_LINKED_ITEM(_menu_messages, "Received",       _menu_main);
    ADD_LINKED_ITEM(_menu_messages, "Send",           _menu_main);
    ADD_LINKED_ITEM(_menu_messages, "Back",           _menu_main);

    ADD_LINKED_ITEM(_menu_periphreal_test, "Buzzer",           _menu_settings);
    ADD_LINKED_ITEM(_menu_periphreal_test, "IMU",           _menu_settings);
    ADD_LINKED_ITEM(_menu_periphreal_test, "Light Sensor",           _menu_settings);
    ADD_LINKED_ITEM(_menu_periphreal_test, "Vibration Motor",           _menu_settings);
    ADD_LINKED_ITEM(_menu_periphreal_test, "Back",           _menu_settings);

    ADD_LINKED_ITEM(_menu_animations_screen, "Off",             _menu_animations);
    ADD_LINKED_ITEM(_menu_animations_screen, "Dance",           _menu_animations);
    ADD_LINKED_ITEM(_menu_animations_screen, "Name",            _menu_animations);
    ADD_LINKED_ITEM(_menu_animations_screen, "Spectrogram",      _menu_animations);
    ADD_LINKED_ITEM(_menu_animations_screen, "Back",             _menu_animations);

    // --- LEVEL 3 (DEEP SUB-SUBMENUS) ---
    // Items inside this leaf menu will link back to the Animations Panel on ESC
    for(uint8_t is_upper=0;is_upper<2;is_upper++)
    {
        ADD_LINKED_ITEM(is_upper?_menu_animations_upper_leds:_menu_animations_lower_leds, "Off",           _menu_animations);
        ADD_LINKED_ITEM(is_upper?_menu_animations_upper_leds:_menu_animations_lower_leds, "Auto Cycle",    _menu_animations);//change every X seconds
        ADD_LINKED_ITEM(is_upper?_menu_animations_upper_leds:_menu_animations_lower_leds, "Blink",         _menu_animations);
        ADD_LINKED_ITEM(is_upper?_menu_animations_upper_leds:_menu_animations_lower_leds, "Fire",          _menu_animations);//note google search animation for 'Meteor'
        ADD_LINKED_ITEM(is_upper?_menu_animations_upper_leds:_menu_animations_lower_leds, "Gyroscope",     _menu_animations);
        ADD_LINKED_ITEM(is_upper?_menu_animations_upper_leds:_menu_animations_lower_leds, "Microphone",    _menu_animations);
        //ADD_LINKED_ITEM(is_upper?_menu_animations_upper_leds:_menu_animations_lower_leds, "Pulse",         _menu_animations);
        ADD_LINKED_ITEM(is_upper?_menu_animations_upper_leds:_menu_animations_lower_leds, "Rainbow Fade",  _menu_animations);
        ADD_LINKED_ITEM(is_upper?_menu_animations_upper_leds:_menu_animations_lower_leds, "Static Green",  _menu_animations);
        ADD_LINKED_ITEM(is_upper?_menu_animations_upper_leds:_menu_animations_lower_leds, "Static Red",    _menu_animations);
        ADD_LINKED_ITEM(is_upper?_menu_animations_upper_leds:_menu_animations_lower_leds, "Steeple Chase", _menu_animations);
        ADD_LINKED_ITEM(is_upper?_menu_animations_upper_leds:_menu_animations_lower_leds, "Stars",         _menu_animations);
        ADD_LINKED_ITEM(is_upper?_menu_animations_upper_leds:_menu_animations_lower_leds, "Back",          _menu_animations);
    }

    #undef ADD_LINKED_ITEM

    // Launch initial application view into root main layout frame
    switch_menu(_menu_main, false);
}

void Graphics::update()
{
    if(_last_update_ms==0) _last_update_ms=millis();//bootup
    uint32_t current_time_ms=millis();
    lv_tick_inc(current_time_ms-_last_update_ms);
    _last_update_ms=current_time_ms;

    lv_timer_handler();
    led_cb(false);

    //memset(_canvas_buffer, 0x66, sizeof(_canvas_buffer)); //<200 us
    //lvgl2spi(_canvas_buffer,_sensor_suite->screen);//direct draw to screen

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
