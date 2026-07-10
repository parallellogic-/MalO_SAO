#include "screen_manager.h"
// Include your actual LVGL header file here (e.g., "lvgl.h" or <lvgl.h>)

ScreenManager::ScreenManager(){}

void ScreenManager::begin(SensorSuite &sensor_suite)
{
  _sensor_suite = &sensor_suite;
  Serial.printf("lv_init START...\n"); delay(10);
  diag();

  lv_init();

  _shared_input_group = lv_group_create();
  Serial.printf("lv_group_create DONE: %p\n",(void*)_shared_input_group); delay(10);
  diag();

  // Configure Display Setup
  lv_display_t * disp = lv_display_create(SCREEN_WIDTH_PX, SCREEN_HEIGHT_PX);
  lv_display_set_buffers(disp, _canvas_buffer, NULL, sizeof(_canvas_buffer), LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_user_data(disp, this); 
  lv_display_set_flush_cb(disp, ScreenManager::_display_flush_cb);

  // Core Game/Level Canvas Bindings
  /*_screen_canvas = (lv_canvas_t*)lv_canvas_create(lv_screen_active());
  lv_canvas_set_buffer((lv_obj_t*)_screen_canvas, _screen_buffer, SCREEN_WIDTH_PX, SCREEN_HEIGHT_PX, LV_COLOR_FORMAT_L8);
  lv_obj_align((lv_obj_t*)_screen_canvas, LV_ALIGN_CENTER, 0, 0);
  lv_obj_add_flag((lv_obj_t*)_screen_canvas, LV_OBJ_FLAG_HIDDEN); */
  _screen_canvas = lv_canvas_create(lv_screen_active());
  if (_screen_canvas != nullptr) {
      // Correct parameter syntax: (object, buffer_ptr, width, height, color_format)
      lv_canvas_set_buffer(_screen_canvas, _screen_buffer, SCREEN_WIDTH_PX, SCREEN_HEIGHT_PX, LV_COLOR_FORMAT_L8);
      
      // Position and hide by default
      lv_obj_align(_screen_canvas, LV_ALIGN_CENTER, 0, 0);
      lv_obj_add_flag(_screen_canvas, LV_OBJ_FLAG_HIDDEN); 
  }
  Serial.printf("lv_canvas_create DONE\n"); delay(10);

  // Input Device Infrastructure
  lv_indev_t * indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_KEYPAD);
  lv_indev_set_user_data(indev, this); 
  lv_indev_set_read_cb(indev, ScreenManager::_button_read_cb);
  //lv_timer_t * read_timer = lv_indev_get_read_timer(indev);
  //if (read_timer != nullptr) lv_timer_set_period(read_timer, 16); 

  lv_obj_set_style_bg_color(lv_screen_active(), lv_color_black(), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(lv_screen_active(), LV_OPA_COVER, LV_PART_MAIN);

  lv_group_set_default(_shared_input_group);
  lv_indev_set_group(indev, _shared_input_group);

  // -- make menu relationships --

  auto main_screen       = std::make_shared<MenuScreen>("Main",_shared_input_group);

  auto animations_screen = std::make_shared<MenuScreen>("Animations",_shared_input_group);  main_screen->add_subscreen(animations_screen);
  auto levels_screen     = std::make_shared<MenuScreen>("Levels",_shared_input_group);      main_screen->add_subscreen(levels_screen);
  auto messages_screen   = std::make_shared<MenuScreen>("Messages",_shared_input_group);    main_screen->add_subscreen(messages_screen);
  auto settings_screen   = std::make_shared<MenuScreen>("Settings",_shared_input_group);    main_screen->add_subscreen(settings_screen);

  auto upper_led_screen  = std::make_shared<MenuScreen>("Upper LEDs",_shared_input_group);  animations_screen->add_subscreen(upper_led_screen);
  auto lower_led_screen  = std::make_shared<MenuScreen>("Lower LEDs",_shared_input_group);  animations_screen->add_subscreen(lower_led_screen);
  auto screen_screen     = std::make_shared<MenuScreen>("Screen",_shared_input_group);      animations_screen->add_subscreen(screen_screen);
  
  
  



Serial.printf("screen_manager._push_screen\n");
  _push_screen(main_screen); //set root menu

  Serial.printf("screen_manager.begin() DONE\n"); delay(10);
}

void ScreenManager::_display_flush_cb(lv_display_t * disp, const lv_area_t * area, uint8_t * px_map) {
  Serial.printf("ScreenManager._display_flush_cb called\n");
    ScreenManager* instance = (ScreenManager*)lv_display_get_user_data(disp);
    if (instance && instance->_sensor_suite) {
      instance->_lvgl2spi((uint8_t*)px_map,instance->_sensor_suite->oled);
    }
    lv_display_flush_ready(disp);
}

void ScreenManager::_button_read_cb(lv_indev_t * indev, lv_indev_data_t * data) {
    ScreenManager* instance = (ScreenManager*)lv_indev_get_user_data(indev);
    if (!instance || !instance->_sensor_suite) return;

    std::shared_ptr<Screen> active_screen=instance->_get_active_screen();
    if(!active_screen) return;

    uint8_t current_button = instance->_sensor_suite->touch.get_down_button();

    if (current_button == 0) {
        instance->_last_raw_button = 0; 
        data->state = LV_INDEV_STATE_RELEASED;
        return;
    }

    data->state = LV_INDEV_STATE_PRESSED;

    // Prevent autofire repeat streams from breaking menu position transitions
    /*if (current_button == instance->_last_raw_button) {
        return; 
    }*/
    instance->_last_raw_button = current_button;

    switch (current_button) {
        case 1:  data->key = LV_KEY_NEXT;  break;//hidden
        case 2:  data->key = LV_KEY_HOME;  break;//menu
        case 3:  data->key = LV_KEY_ESC;   break;//no
        case 4:  data->key = LV_KEY_ENTER; break;//yes
        case 5:  data->key = LV_KEY_ESC;   break;//CCW
        case 6:  data->key = LV_KEY_PREV;  break;//up
        case 7:  data->key = LV_KEY_ENTER; break;//CW
        case 8:  data->key = LV_KEY_LEFT;  break;//left
        case 9:  data->key = LV_KEY_NEXT;  break;//down
        case 10: data->key = LV_KEY_RIGHT; break;//right
        default: break;
    }
    lv_obj_invalidate(lv_screen_active());//work-around for sticky menu that shows selected option at the top of the screen instead of the middle where it should be
}

// Custom function to process the canvas buffer, pack upper nibbles, and transmit
//850 us
void ScreenManager::_lvgl2spi(uint8_t* src,OLED &oled) {
  Serial.printf("ScreenManager._lvgl2spi called\n");
    uint32_t packed_idx = 0;
    
    uint8_t* tx_buffer=oled.get_frame_buffer();
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
    oled.flush();
}

void ScreenManager::update()
{
  Serial.printf("ScreenManager.update called\n");
  if (_screen_stack.empty()) return;

  if(_last_update_ms==0) _last_update_ms=millis();//bootup
  uint32_t current_time_ms=millis();
  lv_tick_inc(current_time_ms-_last_update_ms);
  _last_update_ms=current_time_ms;

  Serial.printf("lv_obj_invalidate\n");
//lv_obj_invalidate(lv_screen_active());//FORCE DRAW every frame

  // Ticks physical interface engine processing every loop frame pass
  uint32_t time_till_next = lv_timer_handler();

//Serial.printf("Next internal task in: %d ms\n", time_till_next); 

  ScreenAction action = _screen_stack.back()->update();

  if (action.type == ScreenActionType::PUSH_SUBMENU) {
      //_screen_stack.back()->end(false);
      _push_screen(action.next_screen);
      //action.next_screen->begin(true);
  } 
  else if (action.type == ScreenActionType::POP_BACK) {
      _pop_screen();
  }
  else if (action.type == ScreenActionType::POP_TO_MENU) {
      // 1. Pop the active screen (the Pause Screen) immediately
      _pop_screen(); 

      // 2. Keep popping until the new top screen is a MenuScreen
      while (_screen_stack.size() > 1) {
          // Peek at the current top screen
          std::shared_ptr<Screen> top_screen = _screen_stack.back();
          
          // Try to safely cast it to a MenuScreen pointer
          if (top_screen != nullptr && top_screen->is_menu()) {
              // Success! Found the menu. Stop popping.
              break; 
          }
          
          // It's not a MenuScreen (e.g., it's the Level screen), pop it!
          _pop_screen();
      }
  }
}

void ScreenManager::end()
{

}

void ScreenManager::_pop_screen() {
  if (_screen_stack.empty()) return;
  _screen_stack.back()->end(true);
  _screen_stack.pop_back();
  if (!_screen_stack.empty()) _screen_stack.back()->begin(false);
}

void ScreenManager::_push_screen(std::shared_ptr<Screen> new_screen)
{
Serial.printf("screen_manager._push_screen 1\n");
  // 1. Safety check to prevent null insertions
  if (new_screen == nullptr) return;

Serial.printf("screen_manager._push_screen 2\n");
  // 2. Lifecycle teardown for the outgoing screen
  if (_get_active_screen() != nullptr) 
  {
    // Tell the current screen it is losing top-level visibility
    // Pass 'false' to indicate user is going deeper into menu structure
    _get_active_screen()->end(false); 
  }

Serial.printf("screen_manager._push_screen 3\n");
  // 3. Track the new screen inside your stack architecture
  // Extract the raw address pointer using .get() to match the vector type
  //_active_screen = new_screen;//.get();
  _screen_stack.push_back(new_screen);

  // 4. Lifecycle startup for the fresh screen
  // Pass 'true' to signal it's entering from above (a fresh push)
  _get_active_screen()->begin(true);
Serial.printf("screen_manager._push_screen 4\n");
}

void ScreenManager::diag(){
    lv_mem_monitor_t mon;
    lv_mem_monitor(&mon);
    Serial.printf("LVGL: Total Pool Size: %d bytes\n", mon.total_size);
    Serial.printf("LVGL: Free Memory Left: %d bytes\n", mon.free_size);
    Serial.printf("LVGL: Memory Used: %d%% (%d bytes)\n", mon.used_pct, mon.total_size - mon.free_size);
    Serial.printf("LVGL: Max Memory Ever Used: %d bytes\n", mon.max_used);
    Serial.printf("LVGL: Memory Fragmentation: %d%%\n", mon.frag_pct);
}