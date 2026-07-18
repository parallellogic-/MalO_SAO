#include "screen_manager.h"
// Include your actual LVGL header file here (e.g., "lvgl.h" or <lvgl.h>)

ScreenManager::ScreenManager(){}

void ScreenManager::_set_menu_structure()
{
  auto main_screen       = std::make_shared<MenuScreen>("Main",_shared_input_group);

  auto animations_screen = std::make_shared<MenuScreen>("Animations",_shared_input_group,ScreenConfig::ANIMATIONS);  main_screen->add_subscreen(animations_screen);
  auto levels_screen     = std::make_shared<MenuScreen>("Games",_shared_input_group);                               main_screen->add_subscreen(levels_screen);
  auto messages_screen   = std::make_shared<MenuScreen>("Messages",_shared_input_group);                             main_screen->add_subscreen(messages_screen);
  auto settings_screen   = std::make_shared<MenuScreen>("Settings",_shared_input_group);                             main_screen->add_subscreen(settings_screen);

  auto upper_led_screen  = std::make_shared<MenuScreen>("Upper LEDs",_shared_input_group,ScreenConfig::LED_UPPER);  animations_screen->add_subscreen(upper_led_screen);
  auto lower_led_screen  = std::make_shared<MenuScreen>("Lower LEDs",_shared_input_group,ScreenConfig::LED_LOWER);  animations_screen->add_subscreen(lower_led_screen);
  auto screen_screen     = std::make_shared<MenuScreen>("Screen",_shared_input_group,ScreenConfig::SCREEN_SAVER);   animations_screen->add_subscreen(screen_screen);
  
  auto tictactoe_screen  = std::make_shared<TicTacToe>("TicTacToe",_shared_input_group);         levels_screen->add_subscreen(tictactoe_screen);
  auto pong_screen       = std::make_shared<Pong>("Pong",_shared_input_group);                   levels_screen->add_subscreen(pong_screen);
  auto snake_screen      = std::make_shared<SnakeGame>("Snake",_shared_input_group);             levels_screen->add_subscreen(snake_screen);
  auto labyrinth_screen  = std::make_shared<LabyrinthGame>("Labyrinth",_shared_input_group);     levels_screen->add_subscreen(labyrinth_screen);
  //Quiz
  //auto box_screen        = std::make_shared<MenuScreen>("Box",_shared_input_group);         levels_screen->add_subscreen(box_screen);
  //auto site19_screen     = std::make_shared<MenuScreen>("Site 19",_shared_input_group);     levels_screen->add_subscreen(site19_screen);
  

  #define INIT_SCREEN_SAVER(var_name, title_str) \
      auto var_name = std::make_shared<ScreenSaver>(title_str, _shared_input_group,&(_sensor_suite->save_state)); \
      screen_screen->add_subscreen(var_name); \
      _screen_savers.push_back(var_name);

  INIT_SCREEN_SAVER(champion_ss,     "Champion");
  INIT_SCREEN_SAVER(chilly_ss,       "Chilly");
  INIT_SCREEN_SAVER(defeated_ss,     "Defeated");
  INIT_SCREEN_SAVER(dance_ss,        "Dance");
  INIT_SCREEN_SAVER(dizzy_ss,        "Dizzy");
  INIT_SCREEN_SAVER(guilty_ss,       "Evil");
  INIT_SCREEN_SAVER(exhausted_ss,    "Exhausted");
  INIT_SCREEN_SAVER(food_ss,         "Favorite Food");
  INIT_SCREEN_SAVER(hacker_ss,       "Hacker BSOD");
  INIT_SCREEN_SAVER(heatwave_ss,     "Heat Wave");
  INIT_SCREEN_SAVER(innocent_ss,     "Innocent");
  INIT_SCREEN_SAVER(quiet_ss,        "It Is Quiet");
  INIT_SCREEN_SAVER(quiet2_ss,       "It Is Too Quiet");
  INIT_SCREEN_SAVER(know_ss,         "Know MalO");
  INIT_SCREEN_SAVER(magnetic_ss,     "Magnetic Personality");
  INIT_SCREEN_SAVER(loser_ss,        "MalO Wins");
  INIT_SCREEN_SAVER(message_rxd_ss,  "Message Received");
  INIT_SCREEN_SAVER(message_sent_ss, "Message Sent");
  INIT_SCREEN_SAVER(lean_ss,         "Snooper Booper");
  INIT_SCREEN_SAVER(tanning_ss,      "Soaking Up Rays");
  INIT_SCREEN_SAVER(winner_ss,       "Winner");


  auto mount_usb_screen  = std::make_shared<MenuScreen>("Mount USB",_shared_input_group,ScreenConfig::MOUNT_USB);     settings_screen->add_subscreen(mount_usb_screen);
  //haptic
  //message motor
  //
  
  auto pause_screen  = std::make_shared<MenuScreen>("Pause",_shared_input_group);
      tictactoe_screen->add_subscreen(pause_screen);
      pong_screen->add_subscreen(pause_screen);
      snake_screen->add_subscreen(pause_screen);
      labyrinth_screen->add_subscreen(pause_screen);



Serial.printf("screen_manager._push_screen\n");
  _push_screen(main_screen); //set root menu
  //_push_screen(levels_screen);
  //_push_screen(tictactoe_screen);
  //_push_screen(pause_screen);

  //_push_screen(animations_screen);
  //_push_screen(screen_screen);
}

void ScreenManager::begin(SensorSuite &sensor_suite)
{
  _sensor_suite = &sensor_suite;
  //while(1){ Serial.printf("ScreenManager::begin(SensorSuite &sensor_suite): %p\n",_sensor_suite); delay(100); }
  //Serial.printf("lv_init START...\n"); delay(10);
  //diag();

  lv_init();

  _shared_input_group = lv_group_create();
  //Serial.printf("lv_group_create DONE: %p\n",(void*)_shared_input_group); delay(10);
  //diag();

  // Configure Display Setup
  lv_display_t * disp = lv_display_create(SCREEN_WIDTH_PX, SCREEN_HEIGHT_PX);
  lv_display_set_buffers(disp, _canvas_buffer, NULL, sizeof(_canvas_buffer), LV_DISPLAY_RENDER_MODE_FULL);
  lv_display_set_user_data(disp, this); 
  lv_display_set_flush_cb(disp, ScreenManager::_display_flush_cb);

  // Core Game/Level Canvas Bindings
  /*_screen_canvas = (lv_canvas_t*)lv_canvas_create(lv_screen_active());
  lv_canvas_set_buffer((lv_obj_t*)_screen_canvas, _screen_buffer, SCREEN_WIDTH_PX, SCREEN_HEIGHT_PX, LV_COLOR_FORMAT_L8);
  lv_obj_align((lv_obj_t*)_screen_canvas, LV_ALIGN_CENTER, 0, 0);
  lv_obj_add_flag((lv_obj_t*)_screen_canvas, LV_OBJ_FLAG_HIDDEN); */
  
  /*_screen_canvas = lv_canvas_create(lv_screen_active());
  if (_screen_canvas != nullptr) {
      // Correct parameter syntax: (object, buffer_ptr, width, height, color_format)
      lv_canvas_set_buffer(_screen_canvas, _screen_buffer, SCREEN_WIDTH_PX, SCREEN_HEIGHT_PX, LV_COLOR_FORMAT_L8);
      
      // Position and hide by default
      lv_obj_align(_screen_canvas, LV_ALIGN_CENTER, 0, 0);
      lv_obj_add_flag(_screen_canvas, LV_OBJ_FLAG_HIDDEN); 
  }*/

  //Serial.printf("lv_canvas_create DONE\n"); delay(10);

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

  _set_menu_structure();

  // === NEW: Initialize Header Structure Object ===
  _system_header = std::make_unique<Header>(_sensor_suite);
  _system_header->begin();

  _achievement_manager=std::make_unique<AchievementManager>(_sensor_suite);
  _achievement_manager->begin();

  //Serial.printf("screen_manager.begin() DONE\n"); delay(10);
}

void ScreenManager::_display_flush_cb(lv_display_t * disp, const lv_area_t * area, uint8_t * px_map) {
  //Serial.printf("ScreenManager._display_flush_cb called\n");
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

    data->key = instance->_get_active_screen()->touch_to_key(current_button);
    /*switch (current_button) {
        case 1:  data->key = 0;            break;//hidden
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
    }*/
    //lv_obj_invalidate(lv_screen_active());//work-around for sticky menu that shows selected option at the top of the screen instead of the middle where it should be
}

// Custom function to process the canvas buffer, pack upper nibbles, and transmit
//850 us
void ScreenManager::_lvgl2spi(uint8_t* src,OLED &oled) {
  //Serial.printf("ScreenManager._lvgl2spi called\n");
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
  //Serial.printf("ScreenManager.update called\n");
  if (_screen_stack.empty()) return;

    lv_mem_monitor_t mon;
    lv_mem_monitor(&mon);
  _sensor_suite->lvgl_memory_percent=mon.used_pct;
  _sensor_suite->lvgl_memory_fragmentation=mon.frag_pct;

  if(_last_update_ms==0) _last_update_ms=millis();//bootup
  uint32_t current_time_ms=millis();
  lv_tick_inc(current_time_ms-_last_update_ms);
  _last_update_ms=current_time_ms;

  _achievement_manager->update();
  if(_screen_stack.back()->is_allow_achivement_popup())
  {
    const std::string* achievement_str= _sensor_suite->save_state.get_first_unseen_achievement();
    if(achievement_str!=nullptr)
    {//achivement pop-up
      //auto achievement_ss = std::make_shared<ScreenSaver>(*achievement_str, _shared_input_group,&(_sensor_suite->save_state),true);
      for(auto achievement_ss : _screen_savers)
      {
        if(achievement_ss->get_title()==*achievement_str)
        {
          //while(1){Serial.printf("HERE PP:  %p %s\n",achievement_ss.get(),achievement_str->c_str()); delay(100); }
          _sensor_suite->save_state.mark_as_seen(*achievement_str);
          //Serial.printf("Save file..."); delay(3);
          //_sensor_suite->save_state.save(); //causes lock-up on core 0/1?
          //Serial.printf("File saved..."); delay(3);
          achievement_ss->set_title_visible(true);
          _push_screen(achievement_ss);
        }
      }

      // This forces the processor to start a clean new loop pass next frame,
      // giving LVGL room to map memory allocations before invoking ScreenSaver::update()
      //return;
    }
  }

  // Ticks physical interface engine processing every loop frame pass
  uint32_t time_till_next = lv_timer_handler();

//Serial.printf("Next internal task in: %d ms\n", time_till_next); 

  ScreenAction action = _screen_stack.back()->update();
  //Serial.printf("ScreenManager.update type %d\n",action.type);
  bool current_screen_allows_header = _screen_stack.back()->is_header();
  if(_system_header != nullptr) _system_header->update(current_screen_allows_header);

  //fetch update to led generation function, if any
  if(action.led_upper_func != nullptr) _led_upper_func=action.led_upper_func;
  if(action.led_lower_func != nullptr) _led_lower_func=action.led_lower_func;

  //Serial.printf("ScreenManager LED: %p, %p\n",_led_upper_func,_led_lower_func);

  //push function live to leds on every frame
  // 2. Handle Upper LED Animation Channel Execution
  if (_led_upper_func != nullptr) {
      // Call the function pointer directly on the object instance using .*
      (_sensor_suite->led_upper.*(_led_upper_func))(*_sensor_suite);
      _sensor_suite->led_upper.flush();
  }

  // 1. Handle Lower LED Animation Channel Execution
  if (_led_lower_func != nullptr) {
      // Call the function pointer directly on the object instance using .*
      (_sensor_suite->led_lower.*(_led_lower_func))(*_sensor_suite);
      _sensor_suite->led_lower.flush();
  }

  if (action.type == ScreenActionType::PUSH_SUBMENU) {
      //_screen_stack.back()->end(false);
      _push_screen(action.next_screen);
      //action.next_screen->begin(true);
  } 
  else if (action.type == ScreenActionType::POP_BACK) {
      _pop_screen();
  }
  else if (action.type == ScreenActionType::POP_TO_MENU || action.type == ScreenActionType::POP_TO_TOP) {
      // 1. Pop the active screen (the Pause Screen) immediately
      _pop_screen(); 

      // 2. Keep popping until the new top screen is a MenuScreen
      while (_screen_stack.size() > 1) {
          // Peek at the current top screen
          std::shared_ptr<Screen> top_screen = _screen_stack.back();
          
          // Try to safely cast it to a MenuScreen pointer
          if (action.type == ScreenActionType::POP_TO_MENU && top_screen != nullptr && top_screen->is_menu()) {
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
  if (!_screen_stack.empty()) _screen_stack.back()->begin(false,_sensor_suite);
}

void ScreenManager::_push_screen(std::shared_ptr<Screen> new_screen)
{
//Serial.printf("screen_manager._push_screen 1\n");
  // 1. Safety check to prevent null insertions
  if (new_screen == nullptr) return;

//Serial.printf("screen_manager._push_screen 2\n");
  // 2. Lifecycle teardown for the outgoing screen
  if (_get_active_screen() != nullptr) 
  {
    // Tell the current screen it is losing top-level visibility
    // Pass 'false' to indicate user is going deeper into menu structure
    _get_active_screen()->end(false); 
  }

//Serial.printf("screen_manager._push_screen 3\n");
  // 3. Track the new screen inside your stack architecture
  // Extract the raw address pointer using .get() to match the vector type
  //_active_screen = new_screen;//.get();
  _screen_stack.push_back(new_screen);

  // 4. Lifecycle startup for the fresh screen
  // Pass 'true' to signal it's entering from above (a fresh push)
  _get_active_screen()->begin(true,_sensor_suite);
//Serial.printf("screen_manager._push_screen 4\n");
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

// ---- Achievement Manager

AchievementManager::AchievementManager(SensorSuite* sensor_suite):_sensor_suite(sensor_suite){}

void AchievementManager::begin(){
      potentiometer=_sensor_suite->analog.get_potentiometer();
}

void AchievementManager::update()
{
  //Serial.printf("457 MenuScreen::AchievementManager::update: %d, %p\n",_sensor_suite->save_state.is_unlocked("Snooper Booper"),_sensor_suite->save_state);
  bool is_booper=_sensor_suite->touch.get_down_button()==1;
  booper.is_sustained(is_booper);
  if(!is_booper && booper.was_sustained()) _sensor_suite->save_state.unlock("Snooper Booper"); //unlock on button release

  float hall_reading=_sensor_suite->analog.get_hall();
  bool is_hall=hall_reading>0.5 || hall_reading<-0.5;
  if(hall.is_sustained(is_hall)) _sensor_suite->save_state.unlock("Magnetic Personality");

  float sound=_sensor_suite->microphone.get_mean_square();
  Serial.printf("Sound: %f\n",sound);
  bool is_sound=sound>500;//halfway between 0 and 127 (max) reading is 22 (in log2 space).  22*22 ~=500.  so mi-log range is cutoff for audio level
  if(music.is_sustained(is_sound)) _sensor_suite->save_state.unlock("Dance");

  bool is_pot=abs(_sensor_suite->analog.get_potentiometer()-potentiometer)>0.2;
  if(is_pot) _sensor_suite->save_state.unlock("Screwing Around");
}