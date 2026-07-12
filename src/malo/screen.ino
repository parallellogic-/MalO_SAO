#include "screen.h"

lv_style_t MenuScreen::_style_main;
lv_style_t MenuScreen::_style_focused;
bool MenuScreen::_styles_initialized = false;
uint8_t ScreenSaver::_pixel_list[SCREEN_WIDTH_PX*SCREEN_HEIGHT_PX];

Screen::Screen(const std::string& title, lv_group_t* shared_input_group,ScreenConfig screen_config): _title(title), _input_group(shared_input_group), _screen_config(screen_config) {
  //Serial.printf("Screen START\n"); delay(10);
}

void Screen::begin(bool is_enter_from_above)
{
  //Serial.printf("Screen::begin called\n");

  // 1. Only handle visibility flags here
  if (_lv_panel != nullptr)
  {
    lv_obj_remove_flag(_lv_panel, LV_OBJ_FLAG_HIDDEN);
    // REMOVED: lv_obj_clean(_lv_panel); -> Destroying widgets belongs strictly in end()!
  }
  
  // 2. Clear input flags safely. Let the derived child screen handle its own group focus assignment
  if (_input_group != nullptr)
  {
    lv_group_remove_all_objs(_input_group);
  }
  _update_action.type=ScreenActionType::NONE;
  _update_action.led_upper_func=nullptr;
  _update_action.led_lower_func=nullptr;

  //_on_focus(_input_group);
  //lv_group_focus_obj(_lv_panel);
}

ScreenAction Screen::update()
{
  ScreenAction action;
  memcpy(&action,&_update_action,sizeof(_update_action));
  //update_action.type=_next_screen_action;
  //_next_screen_action=ScreenActionType::NONE;
  if(action.type==ScreenActionType::PUSH_SUBMENU)
  {
      action.next_screen=_next_screen.lock();
      _next_screen.reset();//release pointer
  }
  _update_action={ScreenActionType::NONE}; //reset for next frame
  return action; // Stay on this screen
  //return { ScreenActionType::NONE }; // Stay on this screen
}


void Screen::end(bool is_leaving_upward)
{
  //Serial.printf("Screen::end called %d\n", is_leaving_upward);

  // 1. Hide the panel container immediately
  // This removes it from the active rendering tree so it stops drawing
  if (_lv_panel != nullptr)
  {
    lv_obj_add_flag(_lv_panel, LV_OBJ_FLAG_HIDDEN);
  }

  // 2. Disconnect the physical controls from this screen frame completely
  // This guarantees that navigation button presses don't fire events 
  // on a screen that is currently closing or fading away.
  if (_input_group != nullptr)
  {
    lv_group_remove_all_objs(_input_group);
  }

  //_on_blur(_input_group);
  //lv_group_focus_obj(nullptr);
}


//void Screen::_on_focus(lv_group_t* input_group){}

// ---- Menu ----

MenuScreen::MenuScreen(const std::string& title, lv_group_t* shared_input_group,ScreenConfig screen_config): Screen(title,shared_input_group,screen_config)
{

  _is_menu=true;
  _init_styles();

  // Create the baseline container panel matching your layout specifications
  _lv_panel = lv_obj_create(lv_screen_active()); 
  lv_obj_set_size(_lv_panel, SCREEN_WIDTH_PX, SCREEN_HEIGHT_PX); 
  lv_obj_set_flex_flow(_lv_panel, LV_FLEX_FLOW_COLUMN); 
  lv_obj_add_flag(_lv_panel, LV_OBJ_FLAG_OVERFLOW_VISIBLE); 
  lv_obj_add_flag(_lv_panel, LV_OBJ_FLAG_SCROLLABLE); 
  lv_obj_remove_flag(_lv_panel, LV_OBJ_FLAG_SCROLL_ELASTIC); 
  lv_obj_remove_flag(_lv_panel, LV_OBJ_FLAG_SCROLL_MOMENTUM); 
  lv_obj_set_scrollbar_mode(_lv_panel, LV_SCROLLBAR_MODE_OFF); 
  lv_obj_set_style_bg_color(_lv_panel, lv_color_black(), LV_PART_MAIN); 
  lv_obj_set_style_border_width(_lv_panel, 0, LV_PART_MAIN); 
  lv_obj_set_style_pad_all(_lv_panel, 0, LV_PART_MAIN); 

  // FIX: Prevent objects from violently forcing adjustments onto your viewport coordinates
  //lv_obj_remove_flag(_lv_panel, LV_OBJ_FLAG_SCROLL_ON_FOCUS); 
  // Ensure snap behaviors are entirely deactivated
  //lv_obj_set_scroll_snap_y(_lv_panel, LV_SCROLL_SNAP_NONE);
  
  // Hide panel initially until requested via begin()
  lv_obj_add_flag(_lv_panel, LV_OBJ_FLAG_HIDDEN);
}

void MenuScreen::_append_menu_item(const std::shared_ptr<Screen>& subscreen,const std::string& title)
{
    lv_obj_t * lbl = lv_label_create(_lv_panel); 
    lv_label_set_text(lbl, title.c_str());
    lv_obj_set_width(lbl, LV_PCT(100)); 
    lv_obj_add_flag(lbl, LV_OBJ_FLAG_CLICKABLE); 
    lv_obj_add_style(lbl, &_style_main, LV_STATE_DEFAULT); 
    lv_obj_add_style(lbl, &_style_focused, LV_STATE_FOCUSED); 
    lv_obj_add_event_cb(lbl, _menu_focus_cb, LV_EVENT_FOCUSED, NULL);
    lv_obj_add_event_cb(lbl, _menu_event_cb, LV_EVENT_ALL, NULL);

    // 2. Allocate it on the heap and bundle your pointers
    //ScreenContext* context = new ScreenContext{ subscreen.get(), this };
    ScreenContext* context = new ScreenContext{ subscreen, this };

    // 3. Save the single container pointer into LVGL
    lv_obj_set_user_data(lbl, context);

    //lv_obj_add_event_cb(lbl, _lv_menu_item_event_cb, LV_EVENT_CLICKED, subscreen);
    lv_obj_add_event_cb(lbl, _lv_menu_item_event_cb, LV_EVENT_CLICKED, context);
    lv_group_add_obj(_input_group, lbl);

    _menu_items.push_back(lbl);
}

void MenuScreen::begin(bool is_enter_from_above)
{
  //Serial.printf("MenuScreen.begin called %d\n",is_enter_from_above);
  Screen::begin(is_enter_from_above);

  if(
    _screen_config!=ScreenConfig::ANIMATIONS &&
    _screen_config!=ScreenConfig::LED_UPPER &&
    _screen_config!=ScreenConfig::LED_LOWER &&
    _screen_config!=ScreenConfig::SCREEN_SAVER
    )
  {
    _update_action.led_upper_func=&Charlieplex::animation_off;
    _update_action.led_lower_func=&Charlieplex::animation_menu_depth;
  }

  if(is_enter_from_above)
  {
    _saved_menu_index = 0; // Reset focus to top when entering a completely new menu instance

    _menu_items.clear(); // Wipe out pointers from previous allocations

    // 2. Loop through child pointers and dynamically instantiate UI elements
    for (const std::shared_ptr<Screen>& subscreen : _screen_stack) {
        _append_menu_item(subscreen,subscreen->get_title());
    }

    if(_screen_config==ScreenConfig::LED_UPPER || _screen_config==ScreenConfig::LED_LOWER)
    {
      for(uint8_t iter=0;iter<Charlieplex::get_animation_count();iter++) _append_menu_item(nullptr,Charlieplex::get_animation_at(iter));
    }

    if(_is_pause_menu())
    {
      _append_menu_item(nullptr,"Resume");
      _append_menu_item(nullptr,"Exit");
    }
    else if(!_is_top_menu())
    {//add back button if there is somewhere up the user can go
        _append_menu_item(nullptr,"Back");
    }
  }
  else 
  {
    // =======================================================
    // FIX: RETURNING FROM A SUBMENU
    // =======================================================
    // Re-add your existing saved buttons back into the control engine
    if (_input_group) {
        for (lv_obj_t* lbl : _menu_items) {
            if (lbl != nullptr) {
                lv_group_add_obj(_input_group, lbl);
            }
        }
    }


  }

  // Focus the initial topmost list option
  /*if (lv_obj_get_child_cnt(_lv_panel) > 0 && _input_group) {
      lv_group_focus_obj(lv_obj_get_child(_lv_panel, 0));
  }*/

    // =======================================================
    // FIX: RESTORE SPECIFIC ITEM FOCUS
    // =======================================================
    if (_input_group && !_menu_items.empty()) {
        // Bounds check safety check to prevent out-of-bounds crashes
        if (_saved_menu_index >= _menu_items.size()) {
            _saved_menu_index = 0;
        }
        
        // Directly focus the tracked menu item from our vector array
        lv_group_focus_obj(_menu_items[_saved_menu_index]);
    }


  /*if (_input_group) {
      _on_focus(_input_group); 
  }*/

}

ScreenAction MenuScreen::update()
{
    /*if (user_selected_brightness) {
      // Instantiate the submenu
      auto sub = std::make_shared<BrightnessScreen>(); 
      return { ScreenAction::PUSH_SUBMENU, sub };
  }*/

  //POP_BACK
  /*ScreenAction action;
  memcpy(&action,&_update_action,sizeof(_update_action));
  //update_action.type=_next_screen_action;
  //_next_screen_action=ScreenActionType::NONE;
  if(action.type==ScreenActionType::PUSH_SUBMENU)
  {
      action.next_screen=_next_screen.lock();
      _next_screen.reset();//release pointer
  }
  _update_action={ScreenActionType::NONE}; //reset for next frame
  return action; // Stay on this screen*/
  return Screen::update();
}

/*void MenuScreen::end(bool is_leaving_upward)
{
  Serial.printf("MenuScreen::end called %d\n",is_leaving_upward);
    if(is_leaving_upward)
    {
      _menu_items.clear(); 
    }
    Screen::end(is_leaving_upward);
}*/

void MenuScreen::end(bool is_leaving_upward)
{
    //Serial.printf("MenuScreen.end called %d\n", is_leaving_upward);
    
    if (!is_leaving_upward && _input_group) 
    {
        lv_obj_t* focused_obj = lv_group_get_focused(_input_group);
        _saved_menu_index = 0; 

        for (size_t i = 0; i < _menu_items.size(); ++i) 
        {
            if (_menu_items[i] == focused_obj) 
            {
                _saved_menu_index = i;
                break;
            }
        }
    }

    // 1. Call base class end() lifecycle logic first
    Screen::end(is_leaving_upward);

    // 2. Only clean up allocations if we are moving away from this screen frame completely
    if (is_leaving_upward) 
    {
        // Remove focus objects from the input engine to prevent ghost inputs
        if (_input_group) {
            lv_group_remove_all_objs(_input_group);
        }

        // Loop through all generated UI elements to dismantle custom contexts and widgets
        for (lv_obj_t* lbl : _menu_items) 
        {
            if (lbl != nullptr) 
            {
                // Extract the raw heap allocation we generated via "new ScreenContext" in _append_menu_item
                ScreenContext* context = static_cast<ScreenContext*>(lv_obj_get_user_data(lbl));
                if (context != nullptr) 
                {
                    delete context; // CRITICAL: Free the struct memory to prevent memory leaks!
                    lv_obj_set_user_data(lbl, nullptr); // Clear the tracking handle
                }
                
                // Completely erase the widget and free its inner text graphics footprint from LVGL's pool
                lv_obj_del(lbl); 
            }
        }

        // Clear out raw tracking pointers since the underlying widgets are destroyed
        _menu_items.clear(); 
    }
}

MenuScreen::~MenuScreen()
{

}

void MenuScreen::_init_styles() {
    if (_styles_initialized) return;
    lv_style_init(&_style_main);
    lv_style_set_bg_opa(&_style_main, LV_OPA_TRANSP); 
    lv_style_set_text_color(&_style_main, lv_color_white());
    lv_style_set_pad_ver(&_style_main, 6);
    lv_style_set_pad_hor(&_style_main, 6);
    
    lv_style_init(&_style_focused);
    lv_style_set_bg_opa(&_style_focused, LV_OPA_COVER);
    lv_style_set_bg_color(&_style_focused, lv_color_white());
    lv_style_set_text_color(&_style_focused, lv_color_black());
    lv_style_set_radius(&_style_focused, 4);
    _styles_initialized = true;
}

// Static event bridge matching LVGL requirements
void MenuScreen::_lv_menu_item_event_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_CLICKED) {

        // Retrieve the screen object mapping bound to this specific list entry
        //Screen* target_screen = (Screen*)lv_event_get_user_data(e).target_subscreen;
        //MenuScreen* parent_menu = (MenuScreen*)lv_obj_get_user_data(lv_event_get_target(e)).host_menu;
        //ScreenContext* context = static_cast<ScreenContext*>(lv_obj_get_user_data(lv_event_get_target(e)));
        ScreenContext* context = static_cast<ScreenContext*>(lv_obj_get_user_data(static_cast<lv_obj_t*>(lv_event_get_target(e))));

        if (context != nullptr) {
            // 2. Extract the targets safely using arrow (->) syntax
            // Since target_subscreen is a shared_ptr, use .get() for a raw Screen*
            std::shared_ptr<Screen> target_screen   = context->target_subscreen;//.get(); 
            MenuScreen* parent_menu = context->host_menu;

            //lv_obj_t * btn = static_cast<lv_obj_t*>(lv_event_get_target(e));
            //std::string button_text = parent_menu->get_title();//lv_obj_get_child(btn, 0); //
            //Serial.printf("Event C %s\n",button_text.c_str());

            lv_obj_t * lbl = static_cast<lv_obj_t*>(lv_event_get_target(e));
            const char* raw_lbl_text = lv_label_get_text(lbl);
            std::string button_text = raw_lbl_text ? raw_lbl_text : "";
            //Serial.printf("Event D %s\n", button_text.c_str());

            if(button_text=="Back" || button_text=="Resume")
            {
                //parent_menu->_next_screen_action=ScreenActionType::POP_BACK;
                parent_menu->_update_action.type=ScreenActionType::POP_BACK;
                return;
            }
            if(button_text=="Exit")
            {
                //parent_menu->_next_screen_action=ScreenActionType::POP_TO_MENU;
                parent_menu->_update_action.type=ScreenActionType::POP_TO_MENU;
                return;
            }
            if(parent_menu->_screen_config==ScreenConfig::LED_UPPER || parent_menu->_screen_config==ScreenConfig::LED_LOWER)
            {
                AnimationFunc afunc=nullptr;
                bool is_found=Charlieplex::get_animation_by_name(button_text,afunc);
                if(is_found)
                {
                  if(parent_menu->_screen_config==ScreenConfig::LED_UPPER) parent_menu->_update_action.led_upper_func=afunc;
                  else                                                     parent_menu->_update_action.led_lower_func=afunc;
                }
            }
            if(target_screen->get_screen_config()==ScreenConfig::MOUNT_USB)
            {
              UniversalSerialBus::set_mounted();
              //while(1){ Serial.printf("HERE: %s, %d\n",target_screen->get_title().c_str(),target_screen->get_screen_config()); delay(100); }
            }


        
            if (target_screen && parent_menu) {
                //parent_menu->handle_selection(target_screen);
                //parent_menu->_next_screen_action=ScreenActionType::PUSH_SUBMENU;
                parent_menu->_next_screen=target_screen;
                parent_menu->_update_action.type=ScreenActionType::PUSH_SUBMENU;
            }
        }
    }
}

/*void MenuScreen::_on_focus(lv_group_t* input_group) {
    lv_group_remove_all_objs(input_group); // Clear old screen focus
    for (auto* item : _menu_items) {
        if(item!=nullptr) lv_group_add_obj(input_group, item); // Add this menu's buttons
    }
}*/

void MenuScreen::_menu_focus_cb(lv_event_t * e) {
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

void MenuScreen::_menu_event_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * obj = (lv_obj_t*)lv_event_get_target(e); 

    // ==========================================
    // 1. DYNAMIC NAVIGATION TRAVERSAL ENGINE
    // ==========================================

    //MenuScreen* instance = (MenuScreen*)lv_display_get_user_data(NULL);
    //if (!instance) return;

    ScreenContext* context = static_cast<ScreenContext*>(lv_obj_get_user_data(static_cast<lv_obj_t*>(lv_event_get_target(e))));
    if (!context) return;

    // 2. Extract the targets safely using arrow (->) syntax
    // Since target_subscreen is a shared_ptr, use .get() for a raw Screen*
    //std::shared_ptr<Screen> target_screen   = context->target_subscreen;//.get(); 
    MenuScreen* instance = context->host_menu;

    if (code == LV_EVENT_KEY) {
        uint32_t key = lv_event_get_key(e);

        if (key == LV_KEY_ESC) {
            // Fetch our embedded back-link hidden inside this label's user data
            //lv_obj_t * parent_menu = (lv_obj_t*)lv_obj_get_user_data(obj);
            
            // If a link exists, seamlessly back up exactly 1 level deep, preserving history
            /*if (parent_menu) {
                instance->switch_menu(parent_menu, true);
                instance->led_cb(true); //turn off leds if leaving the Animations menu
            }*/
            if(!instance->_is_top_menu())//instance->_next_screen_action=ScreenActionType::POP_BACK;
                instance->_update_action.type=ScreenActionType::POP_BACK;
            return;
        }
        
        if (key == LV_KEY_HOME) {
            // Direct escape straight to the root menu frame from any depth layer
            /*if (instance->_active_menu != instance->_menu_main) {
                //instance->switch_menu(instance->_menu_main, true);
                //instance->led_cb(true); //turn off leds if leaving the Animations menu
                
            }*/
            if(!instance->_is_top_menu()) //instance->_next_screen_action=ScreenActionType::POP_TO_TOP;
                instance->_update_action.type=ScreenActionType::POP_TO_TOP;
            return;
        }
    }
}


// ---- screen saver (achievemnt animation) ----

ScreenSaver::ScreenSaver(const std::string& title, lv_group_t* shared_input_group): Screen(title,shared_input_group)
{
  _is_menu=false;
  _is_header = false;

  // Create the baseline container panel matching your layout specifications
  //_lv_panel = lv_obj_create(lv_screen_active()); 
  //_lv_panel = lv_obj_create(lv_screen_active());  //lv_canvas_create  lv_obj_create
  _lv_panel = lv_canvas_create(lv_screen_active());  //lv_canvas_create  lv_obj_create
  lv_obj_set_size(_lv_panel, SCREEN_WIDTH_PX, SCREEN_HEIGHT_PX); 

  // 3. FORCE NO SCROLLBARS: Remove scroll-monitoring overheads completely
  lv_obj_remove_flag(_lv_panel, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(_lv_panel, LV_OBJ_FLAG_SCROLL_ELASTIC);
  lv_obj_set_scrollbar_mode(_lv_panel, LV_SCROLLBAR_MODE_OFF);

  // 4. CLEAN GEOMETRY ALIGNMENT: Strip padding, borders, and margins to prevent offsets
  lv_obj_set_style_pad_all(_lv_panel, 0, LV_PART_MAIN);
  lv_obj_set_style_border_width(_lv_panel, 0, LV_PART_MAIN);
  lv_obj_set_style_margin_all(_lv_panel, 0, LV_PART_MAIN);

  lv_obj_set_style_bg_color(_lv_panel, lv_color_black(), LV_PART_MAIN); 
  
  lv_obj_add_flag(_lv_panel, LV_OBJ_FLAG_CHECKABLE); 
  lv_obj_add_flag(_lv_panel, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(_lv_panel, _screensaver_event_cb, LV_EVENT_ALL, this);

    /*_lv_canvas = lv_canvas_create(_lv_panel); 
    lv_obj_set_size(_lv_canvas, SCREEN_WIDTH_PX, SCREEN_HEIGHT_PX);
    lv_obj_center(_lv_canvas);

    // CRITICAL: Strip clickable flags from canvas so touches fall directly through to the parent panel!
    lv_obj_remove_flag(_lv_canvas, LV_OBJ_FLAG_CLICKABLE);*/

  // Hide panel initially until requested via begin()
  lv_obj_add_flag(_lv_panel, LV_OBJ_FLAG_HIDDEN);

  
}

void ScreenSaver::_screensaver_event_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_KEY && code != LV_EVENT_CLICKED) return; // Ignore all other events like RELEASED (19)
    ScreenSaver* screen = (ScreenSaver*)lv_event_get_user_data(e);
        
    if (screen != nullptr) {
        Serial.printf("ScreenSaver Event Code: %d\n", code);

        bool should_exit = false;

        // 1. IF IT'S A GENERIC KEY EVENT (Fires for PREV, NEXT, and early ENTER)
        if (code == LV_EVENT_KEY) {
            uint32_t key = lv_event_get_key(e);
            Serial.printf("ScreenSaver Key Intercepted: %u\n", key);

            if (key != LV_KEY_ENTER) {
                // If it's a navigation key (PREV/NEXT), it only ever generates this single event.
                // It is 100% safe to exit immediately.
                Serial.println("Wake up triggered by safe navigation key.");
                should_exit = true;
            } else {
                // It is the ENTER key! We explicitly IGNORE its early generic key loop.
                // This lets it pass quietly without triggering a premature screen swap.
                Serial.println("ENTER key loop 1 ignored. Waiting for definitive click...");
            }
        }

        // 2. IF IT'S A CLIMACTIC CLICK EVENT (Fires ONLY for the final phase of ENTER)
        if (code == LV_EVENT_CLICKED) {
            Serial.println("Wake up triggered by definitive ENTER click completion.");
            should_exit = true;
        }

        // 3. EXECUTE EXHAUSTIVE TERMINATION AND EXIT
        if (should_exit) {
            /*lv_indev_t * indev = lv_event_get_indev(e);
            if (indev != nullptr) {
                // Kills the processing token for this frame slice completely, 
                // leaving zero trailing artifacts for downstream consumers.
                lv_indev_stop_processing(indev);
            }*/

            Serial.println("ScreenSaver: Screen exiting cleanly.");
            screen->_update_action.type = ScreenActionType::POP_BACK;
        }
    }
}


void ScreenSaver::begin(bool is_enter_from_above)
{
  Screen::begin(is_enter_from_above);
  if(_input_group != nullptr) lv_group_add_obj(_input_group, _lv_panel); //register button pushes

  _update_action.led_upper_func=&Charlieplex::animation_off; //default to all ledds OFF, can be overriden depending on the animation
  _update_action.led_lower_func=&Charlieplex::animation_off;

  if (is_enter_from_above)
  {
    Serial.printf("ScreenSaver Booting Animation: %s\n", _title.c_str());

    // 2. Clear out our pixel list vector data fields back to flat black
    //std::fill(_pixel_list.begin(), _pixel_list.end(), 0);
    //_pixel_list.resize(SCREEN_WIDTH_PX * SCREEN_HEIGHT_PX); //init's dirty
    //if (_lv_canvas != nullptr) lv_canvas_set_buffer(_lv_canvas, _pixel_list.data(), SCREEN_WIDTH_PX, SCREEN_HEIGHT_PX, LV_COLOR_FORMAT_L8);
    if (_lv_panel != nullptr) lv_canvas_set_buffer(_lv_panel, _pixel_list, SCREEN_WIDTH_PX, SCREEN_HEIGHT_PX, LV_COLOR_FORMAT_L8);

    // 3. Open configuration file path using the parent _title string
    char config_filename[128];
    snprintf(config_filename, sizeof(config_filename), "/animations/%s.dur", _title.c_str());

    File32 duration_file = FlashInterface::fat_fs.open(config_filename, O_RDONLY);
    if (duration_file) {
        // Fetch raw file size properties to allocate vectors exactly to target specs
        uint32_t file_size = duration_file.size();
        _frame_duration.resize(file_size);
        duration_file.read(_frame_duration.data(), file_size);
        Serial.println("ScreenSaver Vectors Config Loaded Perfectly");
    } else {
        Serial.printf("ScreenSaver Warning: Missing layout config file %s\n", config_filename);
        
        // Dynamic Safe Fallback allocations
        _frame_duration = { 6 };   // Hold 1st frame for 6 ticks (100 ms)
    }

    snprintf(config_filename, sizeof(config_filename), "/animations/%s.ord", _title.c_str());

    File32 order_file = FlashInterface::fat_fs.open(config_filename, O_RDONLY);
    if (order_file) {
        // Fetch raw file size properties to allocate vectors exactly to target specs
        uint32_t file_size = order_file.size();
        _frame_order.resize(file_size);
        order_file.read(_frame_order.data(), file_size);
        Serial.println("ScreenSaver Vectors Config Loaded Perfectly");
    } else {
        Serial.printf("ScreenSaver Warning: Missing layout config file %s\n", config_filename);
        
        // Dynamic Safe Fallback allocations
        _frame_order = { 0, 0 }; // Play index 0, then flag structural loop end
    }
  }

  _frame_index=255;//trigger an immediaate redraw upon entering frame
}


ScreenAction ScreenSaver::update()
{
  //fat_fs.chvol(); //unclear if needed?

  Serial.printf("ScreenSaver update\n");
  uint8_t current_frame_index=_get_current_frame();//what should the frame index be within _duration_list?
  bool is_redraw=false;
  if(_frame_index!=current_frame_index)
  {//if mismatch from waht it was, trigger a redraw
    is_redraw=true;
    _frame_index=current_frame_index;//remember which frame was draw for future reference
  }

  if(is_redraw)
  {
    char filename_buffer[64];
    snprintf(filename_buffer, sizeof(filename_buffer), "/animations/%s_%03d.cmp", _title.c_str(), current_frame_index);

    File32 local_file = FlashInterface::fat_fs.open(filename_buffer, O_RDONLY);
    if (local_file) {
          /*if (local_file.size() > (SCREEN_WIDTH_PX * SCREEN_HEIGHT_PX)) {
              local_file.seek(12); //skip lvgl header
          }*/
          //Serial.printf("ScreenSaver local_file %s, %d, %d\n", filename_buffer,local_file.size(),_pixel_list.size()); //ScreenSaver local_file /animations/film_000.cmp, 16396, 16384 - todo: resize phsycial file to right size later...
          //Serial.printf("ScreenSaver _pixel_list  0x%02X, 0x%02X, 0x%02X, 0x%02X\n",_pixel_list[0],_pixel_list[1],_pixel_list[2],_pixel_list[3]);
          local_file.read(_pixel_list, SCREEN_WIDTH_PX * SCREEN_HEIGHT_PX);
          //Serial.printf("ScreenSaver _pixel_list2 0x%02X, 0x%02X, 0x%02X, 0x%02X\n",_pixel_list[0],_pixel_list[1],_pixel_list[2],_pixel_list[3]); //valid contents found
          local_file.close();

          // 4. PUSH PIXELS LIVE TO THE SCREEN VIEWPORT
          // If your ScreenSaver has its own _lv_panel or references the canvas:
          if (_lv_panel != nullptr) {
              // Option A: If _lv_panel is an LVGL Canvas object, copy buffer directly:
              
              // Option B: Force an explicit area invalidation to make ScreenManager draw it
              lv_obj_invalidate(_lv_panel); 
          }
      } else {
          Serial.printf("ScreenSaver Error: Failed to open %s\n", filename_buffer);
      }
  }

  _update_current_frame(); //increment frame index state machine

  return _update_action;
}

uint8_t ScreenSaver::_get_current_frame()
{//index within _frame_duration
  return _frame_order[_frame_order_index];
}

void ScreenSaver::_update_current_frame()
{
  _frame_elapsed++;
  uint8_t frame_duration=_frame_duration[_get_current_frame()];
  if(_frame_elapsed>=frame_duration)
  {
    _frame_elapsed=0;
    _frame_order_index++;
    if(_frame_order_index>=_frame_order.size()-1)
    {//if at end of animation (-1 because the last value is where within _frame_order to jump to)
      _frame_order_index=_frame_order[_frame_order.size()-1];
    }
  }
}

void ScreenSaver::end(bool is_leaving_upward)
{

  if (is_leaving_upward)
  {
    Serial.printf("ScreenSaver Shutting Down Animation: %s\n", _title.c_str());

    // 3. Clear out and shrink dynamic vector memory to avoid heap fragmentation
    //_pixel_list.clear();
    //_pixel_list.shrink_to_fit();
    memset(_pixel_list,0,sizeof(_pixel_list));

    _frame_duration.clear();
    _frame_duration.shrink_to_fit();

    _frame_order.clear();
    _frame_order.shrink_to_fit();

    // 4. Reset safe runtime indexing states
    _frame_index = 255;
  }

  // 5. Always chain up to the base class to allow the layout engine to finalize transition actions
  Screen::end(is_leaving_upward);
}


/*void ScreenSaver::_on_focus(lv_group_t* input_group) {
    // Screensavers don't contain list widgets, so leave this stub empty.
    // This stops it from modifying your active LVGL input configuration groups.
}*/

//gameScreen:
/*   
 void on_focus(lv_group_t* input_group) override {
        lv_group_remove_all_objs(input_group); // Disables LVGL UI selection entirely
    }

    void handle_input(uint32_t key, bool is_pressed) override {
        if (!is_pressed) return;

        switch(key) {
            case LV_KEY_LEFT:  move_player(-1, 0); break;
            case LV_KEY_RIGHT: move_player(1, 0);  break;
            case LV_KEY_ENTER: fire_laser();       break;
        }
    }
*/

