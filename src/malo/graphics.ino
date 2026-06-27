#include "graphics.h"

Graphics::Graphics()
{

}

static lv_draw_buf_t custom_canvas_draw_handle;
static void dummy_display_flush_cb(lv_display_t* disp, const lv_area_t* area, uint8_t* px_map) {
    lv_display_flush_ready(disp);
}

void Graphics::begin()
{
    lv_init();

    // Register internal virtual operational pipeline
    lv_display_t* dummy_disp = lv_display_create(SCREEN_WIDTH_PX, SCREEN_HEIGHT_PX);
    lv_display_set_color_format(dummy_disp, LV_COLOR_FORMAT_L8);
    lv_display_set_flush_cb(dummy_disp, dummy_display_flush_cb);

    // Build raw custom context mapping allocations
    _main_canvas = lv_canvas_create(lv_screen_active());
    lv_draw_buf_init(
        &_custom_canvas_draw_handle, 
        SCREEN_WIDTH_PX, SCREEN_HEIGHT_PX, 
        LV_COLOR_FORMAT_L8, 
        SCREEN_WIDTH_PX, 
        _canvas_buffer, 
        sizeof(_canvas_buffer)
    );
    
    // Bind Canvas Handle directly 
    lv_canvas_set_draw_buf(_main_canvas, &_custom_canvas_draw_handle);

    // Provision base UI segments
    build_base_ui_frame();

}

void Graphics::update(SensorSuite &sensor_suite)
{
    //Serial.println("lv_obj_center");
    //lv_obj_center(_main_canvas); //600 us

    //Serial.println("lv_canvas_fill_bg");
    // 4. Fill the background of the canvas with a baseline color value (e.g., 0x30)
    //lv_canvas_fill_bg(_main_canvas, lv_color_hex(0x333333), LV_OPA_COVER); //1200 us
    memset(_canvas_buffer, 0x00, sizeof(_canvas_buffer)); //<200 us




    // 5. Configure drawing styles for your rectangle
    lv_draw_rect_dsc_t rect_dsc;
    lv_draw_rect_dsc_init(&rect_dsc);
    
    // Map colors to the 8-bit index space
    rect_dsc.bg_color = lv_color_hex(0xCCCCCC); // Target bright pixels (~0xCC grayscale value)
    rect_dsc.bg_opa = LV_OPA_COVER;
    
    // Define an optional border outline for the rectangle
    rect_dsc.border_color = lv_color_hex(0xFFFFFF); // White outline border (~0xFF grayscale value)
    rect_dsc.border_width = 2;

    //Serial.println("lv_canvas_init_layer");
    lv_layer_t layer;
    lv_canvas_init_layer(_main_canvas, &layer);
    // 6. Execute the geometric vector coordinate draw operation onto the canvas
    // Draws a centered 64x64 rectangle inside our 128x128 bounding window
    int16_t offset=sensor_suite.frame_id0%64 -16;
    lv_area_t coords_rect = {32+offset, 32+offset, 32+offset + 64 - 1, 32+offset + 64 - 1};

    // Execute the actual universal draw call onto your canvas layer context
    lv_draw_rect(&layer, &rect_dsc, &coords_rect);
    // Close and flush the canvas rendering context block back down to canvas_buffer
    
    // ----------------------------------------------------------------
    // STEP B: DRAW THE TEXT ON TOP OF THE RECTANGLE
    // ----------------------------------------------------------------
    lv_draw_label_dsc_t label_dsc;
    lv_draw_label_dsc_init(&label_dsc);
    
    // Configure text appearance parameters
    label_dsc.text = "MALO4";                    // The text string to display
    label_dsc.color = lv_color_hex(0xFFFFFF);      // High luminosity white text
    label_dsc.font = LV_FONT_DEFAULT;              // Fallback to built-in system font
    label_dsc.align = LV_TEXT_ALIGN_CENTER;        // Horizontal alignment math

    // Define the bounding coordinate box for the text placement
    // Placing it squarely inside the boundaries of our background rectangle
    lv_area_t coords_text = {20, 52, 108, 76};
    lv_draw_label(&layer, &label_dsc, &coords_text);




    // -- other draw actions --

    lv_canvas_finish_layer(_main_canvas, &layer);//1200 us

    //Serial.println("lv_refr_now");
    // Force an internal update pass so changes are immediately rasterized into canvas_buffer
    lv_refr_now(NULL);//<30 us

    // 7. Extract the data layers, pack the nibbles, and flash the screen
    lvgl2spi(sensor_suite.screen); //600 us



    // 1. Force state check on missing initialization flags
    /*if (sensors.save.data.magic_header != 0xDEADBEEF) {
        provision_default_save(sensors);
        _current_state = MenuState::TOS_PROMPT;
        build_tos_screen();
    }

    // 2. Refresh physical metrics layer layout
    draw_title_bar(sensors);

    // 3. Physical Input Processing Layer
    uint8_t key = sensors.touch.get_down_button();

    // Secret find/unlock boop intercept trigger
    if (key == 1) {
        trigger_achievement_overlay(0); // Trigger secret achievement 0
    }

    // 4. Handle Modal Achievement Overlays
    if (_overlay_active) {
        if (millis() - _overlay_timestamp > 1000) {
            // Render an indicators prompt on top of layout after 1s
            static lv_obj_t* prompt = nullptr;
            if (!prompt) {
                prompt = lv_label_create(_overlay_panel);
                lv_label_set_text(prompt, "Press [YES] to Close");
                lv_obj_align(prompt, LV_ALIGN_BOTTOM_MID, 0, -5);
            }
            if (key == 4) { // 'YES' mapping accepted
                lv_obj_delete(_overlay_panel);
                _overlay_panel = nullptr;
                prompt = nullptr;
                _overlay_active = false;
            }
        }
        // Force rendering loops processing even during screen suspensions
        lv_timer_handler();
        return;
    }

    // 5. High-Level Menu Infrastructure Execution Routing
    switch (_current_state) {
        case MenuState::TOS_PROMPT:
            if (key == 9) { // DOWN Arrow to scroll text
                lv_obj_scroll_by(_content_area, 0, -10, LV_ANIM_ON);
            } else if (key == 6) { // UP Arrow to scroll text
                lv_obj_scroll_by(_content_area, 0, 10, LV_ANIM_ON);
            } else if (key == 4) { // YES Button to accept agreement
                sensors.save.data.tos_accepted = true;
                save_state_to_disk(sensors);
                lv_obj_clean(_content_area);
                _current_state = MenuState::MAIN_MENU;
                const char* main_opts[] = {"Animations", "Levels", "Messages", "Achievements", "Settings"};
                build_menu_tree(main_opts, 5);
            }
            break;

        case MenuState::MAIN_MENU:
            if (key == 2) { }// Show global escape pause metrics if required
            handle_menu_navigation(key);
            if (key == 4) { // Enter options
                lv_obj_clean(_content_area);
                switch (_selected_menu_idx) {
                    case 0: _current_state = MenuState::SUB_ANIMATIONS;   break;
                    case 1: _current_state = MenuState::SUB_LEVELS;        break;
                    case 2: _current_state = MenuState::SUB_MESSAGES;      break;
                    case 3: _current_state = MenuState::SUB_ACHIEVEMENTS;  break;
                    case 4: _current_state = MenuState::SUB_SETTINGS;      break;
                }
                _selected_submenu_idx = 0;
            }
            break;

        case MenuState::SUB_ANIMATIONS:
            // Process submenus: Upper LEDs, Lower LEDs, Screen
            if (key == 3) { // NO / Return Code
                _current_state = MenuState::MAIN_MENU;
                // Re-build Main level elements...
            }
            break;

        case MenuState::ANIM_SCREEN_RUNNING:
            handle_name_anim_input(key, sensors);
            break;

        case MenuState::SUB_LEVELS:
            if (key == 4) {
                initialize_level_subsystem(_selected_submenu_idx);
            }
            break;

        case MenuState::LEVEL_ACTIVE:
            if (key == 2) { // Menu Intercept button triggers game state suspension
                _previous_state = _current_state;
                _current_state = MenuState::LEVEL_PAUSED;
                // Construct Pause Interface widgets overlays: "Resume", "Restart", "Settings", "Exit"
            } else {
                if (_active_level_id == 1) _level_pool.tictactoe.update(sensors);
                if (_active_level_id == 2) _level_pool.invaders.update(sensors);
            }
            break;

        case MenuState::LEVEL_PAUSED:
            // Manage standard selections inside structural parameters configurations
            if (key == 4) { // Execute tracking selection indexes 
                // E.g. structural configuration assignments inside `configure_setting()` logic
            }
            break;
            
        default:
            break;
    }

    // Call inner drawing framework pipelines
    lv_timer_handler();*/
}

void Graphics::end()
{

}

// -- helper methods --


// Custom function to process the canvas buffer, pack upper nibbles, and transmit
//850 us
void Graphics::lvgl2spi(Screen &screen) {
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
            uint8_t left_nibble  = _canvas_buffer[pixel_left_idx]  & 0xF0;
            uint8_t right_nibble = _canvas_buffer[pixel_right_idx] & 0xF0;

            // Pack them perfectly: Left pixel high bits, Right pixel low bits
            tx_buffer[packed_idx++] = left_nibble | (right_nibble >> 4);
        }
    }
    
    // Transmit the fully optimized 4bpp block directly to your display controller
    screen.flush();
}

void Graphics::build_base_ui_frame() {
    // Generate isolated rendering layers on top of base display Canvas hierarchy
    /*_title_bar = lv_obj_create(lv_screen_active());
    lv_obj_set_size(_title_bar, SCREEN_WIDTH_PX, 16);
    lv_obj_align(_title_bar, LV_ALIGN_TOP_MID, 0, 0);
    
    _lbl_battery = lv_label_create(_title_bar);
    _lbl_unlocks = lv_label_create(_title_bar);
    _lbl_ir = lv_label_create(_title_bar);
    lv_obj_align(_lbl_battery, LV_ALIGN_LEFT_MID, 2, 0);
    lv_obj_align(_lbl_unlocks, LV_ALIGN_CENTER, 0, 0);
    lv_obj_align(_lbl_ir, LV_ALIGN_RIGHT_MID, -2, 0);

    _content_area = lv_obj_create(lv_screen_active());
    lv_obj_set_size(_content_area, SCREEN_WIDTH_PX, SCREEN_HEIGHT_PX - 16);
    lv_obj_align(_content_area, LV_ALIGN_BOTTOM_MID, 0, 0);*/
}

void Graphics::draw_title_bar(SensorSuite& sensors) {


    /*char buf[16];
    snprintf(buf, sizeof(buf), "%.2fV", sensors.save.battery_voltage);
    lv_label_set_text(_lbl_battery, buf);

    // Calculate total set flags inside save file records
    uint8_t count = 0;
    for(int i=0; i<32; i++) {
        if(sensors.save.data.unlocks_bitmap & (1 << i)) count++;
    }
    snprintf(buf, sizeof(buf), "L%d", count);
    lv_label_set_text(_lbl_unlocks, buf);

    if (sensors.save.new_ir_received) {
        lv_label_set_text(_lbl_ir, sensors.save.recent_ir_msg);
    }*/
}