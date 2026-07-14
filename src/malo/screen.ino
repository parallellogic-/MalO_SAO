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
  if(is_header())
  {
    lv_obj_set_size(_lv_panel, SCREEN_WIDTH_PX, SCREEN_HEIGHT_PX-HEADER_HEIGHT_PX); 
  }else{
    lv_obj_set_size(_lv_panel, SCREEN_WIDTH_PX, SCREEN_HEIGHT_PX); 
  }
  lv_obj_set_flex_flow(_lv_panel, LV_FLEX_FLOW_COLUMN); 
  lv_obj_add_flag(_lv_panel, LV_OBJ_FLAG_OVERFLOW_VISIBLE); 
  lv_obj_add_flag(_lv_panel, LV_OBJ_FLAG_SCROLLABLE); 
  lv_obj_remove_flag(_lv_panel, LV_OBJ_FLAG_SCROLL_ELASTIC); 
  lv_obj_remove_flag(_lv_panel, LV_OBJ_FLAG_SCROLL_MOMENTUM); 
  lv_obj_set_scrollbar_mode(_lv_panel, LV_SCROLLBAR_MODE_OFF); 
  lv_obj_set_style_bg_color(_lv_panel, lv_color_black(), LV_PART_MAIN); 
  lv_obj_set_style_border_width(_lv_panel, 0, LV_PART_MAIN); 
  lv_obj_set_style_pad_all(_lv_panel, 0, LV_PART_MAIN); 

  if(is_header()) lv_obj_set_style_pad_top(_lv_panel, HEADER_HEIGHT_PX, LV_PART_MAIN);

  // FIX: Prevent objects from violently forcing adjustments onto your viewport coordinates
  //lv_obj_remove_flag(_lv_panel, LV_OBJ_FLAG_SCROLL_ON_FOCUS); 
  // Ensure snap behaviors are entirely deactivated
  //lv_obj_set_scroll_snap_y(_lv_panel, LV_SCROLL_SNAP_NONE);
  
  // Hide panel initially until requested via begin()
  lv_obj_add_flag(_lv_panel, LV_OBJ_FLAG_HIDDEN);
}

/*void MenuScreen::_label_icon_draw_cb(lv_event_t * e) {
    if (e == nullptr) return;

    // 1. Only process during the active rendering window phase
    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_DRAW_MAIN && code != LV_EVENT_DRAW_POST) return;

    lv_obj_t * lbl = static_cast<lv_obj_t*>(lv_event_get_user_data(e));
    if (lbl == nullptr) return;

    lv_layer_t * layer = lv_event_get_layer(e);
    if (layer == nullptr) return;
    
    ScreenContext* context = static_cast<ScreenContext*>(lv_obj_get_user_data(lbl));
    if (!context || !context->target_subscreen) return;
    if (context->host_menu->get_screen_config() != ScreenConfig::SCREEN_SAVER) return;
    std::shared_ptr<ScreenSaver> screensaver = std::static_pointer_cast<ScreenSaver>(context->target_subscreen);

    const lv_image_dsc_t* active_icon = screensaver->is_locked() ? &icon_lock_dsc : &icon_unlock_dsc;

    lv_area_t txt_area;
    lv_obj_get_coords(lbl, &txt_area);
    
    int32_t lbl_h = lv_obj_get_height(lbl);
    if (lbl_h <= 0) return;

    lv_area_t img_area;
    img_area.x1 = txt_area.x1 + 2;
    img_area.y1 = txt_area.y1 + ((lbl_h - 10) / 2);
    img_area.x2 = img_area.x1 + 10 - 1;
    img_area.y2 = img_area.y1 + 10 - 1;

    if (img_area.x2 < img_area.x1 || img_area.y2 < img_area.y1) return;

    // =======================================================
    // ⚡ BYPASS PIPELINE VIA SYNCHRONOUS INLINE LAYER INJECTION
    // =======================================================

    // Initialize an image descriptor variant specifically for layer composition
    lv_draw_image_dsc_t img_dsc;
    lv_draw_image_dsc_init(&img_dsc);
    img_dsc.src = active_icon;

    if (lv_obj_has_state(lbl, LV_STATE_FOCUSED)) {
        // Safe to use here because img_dsc is an image descriptor variant type
        img_dsc.blend_mode = LV_BLEND_MODE_SUBTRACTIVE;
    }

    // Direct synchronous draw injection: paints layer to display instantly
    lv_draw_layer(layer, &img_dsc, &img_area);//hangs intermittently
}*/

void MenuScreen::_label_icon_draw_cb(lv_event_t * e) {
    if (e == nullptr) return;

    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_DRAW_MAIN && code != LV_EVENT_DRAW_POST) return;

    lv_obj_t * lbl = static_cast<lv_obj_t*>(lv_event_get_user_data(e));
    lv_layer_t * layer = lv_event_get_layer(e);
    if (lbl == nullptr || layer == nullptr) return;
    
    ScreenContext* context = static_cast<ScreenContext*>(lv_obj_get_user_data(lbl));
    if (!context || !context->target_subscreen) return;
    if (context->host_menu->get_screen_config() != ScreenConfig::SCREEN_SAVER) return;
    std::shared_ptr<ScreenSaver> screensaver = std::static_pointer_cast<ScreenSaver>(context->target_subscreen);

    // 1. Unpack the raw uint8_t byte array pointer directly from the image descriptor
    const lv_image_dsc_t* active_icon = screensaver->is_locked() ? &icon_lock_dsc : &icon_unlock_dsc;
    const uint8_t* raw_bytes = active_icon->data;
    if (raw_bytes == nullptr) return;

    lv_area_t txt_area;
    lv_obj_get_coords(lbl, &txt_area);
    
    int32_t lbl_h = lv_obj_get_height(lbl);
    if (lbl_h <= 0) return;

    // Calculate baseline top-left starting location for the 10x10 block
    int32_t start_x = txt_area.x1 + 2;
    int32_t start_y = txt_area.y1 + ((lbl_h - 10) / 2);

    // 2. Initialize a geometry rectangle descriptor for a 1x1 solid pixel block
    lv_draw_rect_dsc_t pixel_dsc;
    lv_draw_rect_dsc_init(&pixel_dsc);
    pixel_dsc.bg_color = lv_color_white(); // Or your target color
    
    if (lv_obj_has_state(lbl, LV_STATE_FOCUSED)) {
        // Primitive structures support standard blend modes in v9 through internal hooks
        // If this field errors out, we can handle inversion using solid geometric fills instead
        pixel_dsc.bg_opa = LV_OPA_COVER; 
    } else {
        pixel_dsc.bg_opa = LV_OPA_COVER;
    }

    bool invert=lv_obj_has_state(lbl, LV_STATE_FOCUSED);
    
    // ⚡ DYNAMIC BG COLOR COLORATION SWITCH
    if (invert) {
        // If focused (inverted mode on a white selector block), paint black pixels 
        pixel_dsc.bg_color = lv_color_black(); 
    } else {
        // Normal mode (unfocused): paint white pixels onto the native screen layout
        pixel_dsc.bg_color = lv_color_white(); 
    }

    lv_area_t pixel_area;
    for (int32_t y = 0; y < 10; y++) {
        for (int32_t x = 0; x < 10; x++) {
            uint8_t pixel_intensity = raw_bytes[(y * 10) + x];

            // Skip drawing completely transparent elements
            if (pixel_intensity == 0x00) continue;

            // Map the coordinates for the individual 1x1 point block
            pixel_area.x1 = start_x + x;
            pixel_area.y1 = start_y + y;
            pixel_area.x2 = pixel_area.x1;
            pixel_area.y2 = pixel_area.y1;

            // Assign intensity safely as transparency opacity to preserve smoothing curves
            pixel_dsc.bg_opa = pixel_intensity;

            // Paint the geometry pixel block synchronously 
            lv_draw_rect(layer, &pixel_dsc, &pixel_area);
        }
    }
}

void MenuScreen::_append_menu_item(const std::shared_ptr<Screen>& subscreen,const std::string& title)
{
    lv_obj_t * lbl = lv_label_create(_lv_panel); 
    lv_label_set_text(lbl, title.c_str());

    lv_label_set_text(lbl, title.c_str());
    if(subscreen != nullptr && _screen_config == ScreenConfig::SCREEN_SAVER)
    {
        // Add 3 safe spaces to push the text title words cleanly out of the left gutter's way
        //char buffer[128];
        //snprintf(buffer, sizeof(buffer), "   %s", title.c_str());
        //lv_label_set_text(lbl, title);
        
        // Attach the ScreenContext FIRST so the draw callback can access it
        //ScreenContext* context = new ScreenContext{ subscreen, this };
        //lv_obj_set_user_data(lbl, context);

        // Register the lightweight graphics drawing hook
        lv_obj_add_event_cb(lbl, _label_icon_draw_cb, LV_EVENT_DRAW_MAIN, lbl); //event draw is unstable because lv_draw_layer is intermittent errors.   lv_canvas_create is more immediate failures --> draw one pixel at a time with rect...

        lv_obj_set_style_pad_left(lbl, 16, LV_PART_MAIN); 
    }
    lv_obj_set_style_pad_top(lbl, 5, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(lbl, 5, LV_PART_MAIN);
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

    if(_screen_config==ScreenConfig::ANIMATIONS)
    {
      _update_action.led_upper_func=&Charlieplex::animation_off;
      _update_action.led_lower_func=&Charlieplex::animation_off;
    }

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
                if(parent_menu->get_screen_config()==ScreenConfig::SCREEN_SAVER)
                {
                  std::shared_ptr<ScreenSaver> screensaver = std::static_pointer_cast<ScreenSaver>(target_screen);
                  if (!screensaver->is_locked())
                  {//only show animation if user has unlocked it
                    parent_menu->_next_screen=target_screen;
                    parent_menu->_update_action.type=ScreenActionType::PUSH_SUBMENU;
                  }
                }else{
                  parent_menu->_next_screen=target_screen;
                  parent_menu->_update_action.type=ScreenActionType::PUSH_SUBMENU;
                }
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

ScreenSaver::ScreenSaver(const std::string& title, lv_group_t* shared_input_group,SaveState* save_state,bool is_title_visible): Screen(title,shared_input_group), _save_state(save_state)
{
  _is_menu=false;
  _is_header = false;

  if(title=="Champion" || title=="Favorite Food") _is_title_visible=false;
  else _is_title_visible=is_title_visible;

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

void ScreenSaver::_create_unlock_overlay(lv_obj_t* canvas_obj, const std::string& text_str) {
    if (canvas_obj == nullptr) return;

    lv_obj_t* canvas_parent = lv_obj_get_parent(canvas_obj);
    if (canvas_parent == nullptr) return;

    // Create the base container card
    lv_obj_t* card = lv_obj_create(canvas_parent);
    
    // Save the pointer to our class tracker variable
    _overlay_card = card; 

    lv_obj_set_size(card, 110, 24);                   
    lv_obj_align(card, LV_ALIGN_BOTTOM_MID, 0, -6);    
    
    lv_obj_set_style_pad_all(card, 4, 0);
    lv_obj_set_style_pad_gap(card, 6, 0);             
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_ROW);      
    lv_obj_set_flex_align(card, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scrollbar_mode(card, LV_SCROLLBAR_MODE_OFF);

    lv_obj_set_style_bg_color(card, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(card, 0, 0);        
    lv_obj_set_style_outline_color(card, lv_color_white(), 0);
    lv_obj_set_style_outline_width(card, 1, 0);       
    lv_obj_set_style_radius(card, 3, 0);              

    lv_obj_t* icon_obj = lv_image_create(card);
    lv_image_set_src(icon_obj, &icon_unlock_dsc);

    lv_obj_t* label = lv_label_create(card);
    lv_label_set_text(label, text_str.c_str());
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    if(text_str.length()>15) lv_obj_set_style_text_font(label, &lv_font_montserrat_8, 0); 
    else lv_obj_set_style_text_font(label, &lv_font_montserrat_10, 0); 
}


void ScreenSaver::begin(bool is_enter_from_above)
{
  Screen::begin(is_enter_from_above);
  if(_input_group != nullptr) lv_group_add_obj(_input_group, _lv_panel); //register button pushes

  //_update_action.led_upper_func=&Charlieplex::animation_off; //default to all ledds OFF, can be overriden depending on the animation
  //_update_action.led_lower_func=&Charlieplex::animation_off;

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

    if(_is_title_visible) _create_unlock_overlay(_lv_panel, _title);
  }

  _frame_index=255;//trigger an immediaate redraw upon entering frame
  _frame_order_index=0;
  _frame_elapsed=0;
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
      if (_overlay_card != nullptr) {
          lv_obj_delete(_overlay_card);
          _overlay_card = nullptr; // Reset to prevent double-free crashes
      }

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
    _frame_order_index=0;
    _frame_elapsed=0;
  }

  // 5. Always chain up to the base class to allow the layout engine to finalize transition actions
  Screen::end(is_leaving_upward);
}

// ---------------- Header ----------------------------

Header::Header(SensorSuite* sensor_suite) : _sensor_suite(sensor_suite) {}


void Header::begin() {
    if (_sensor_suite == nullptr) return;

    // 1. Create top-aligned header strip (Height = 10px across full screen width)
    _header_container = lv_obj_create(lv_screen_active());
    lv_obj_set_size(_header_container, LV_PCT(100), HEADER_HEIGHT_PX);
    lv_obj_align(_header_container, LV_ALIGN_TOP_MID, 0, 0);
    
    // Apply styling
    lv_obj_set_style_bg_color(_header_container, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(_header_container, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(_header_container, 0, 0);
    lv_obj_set_style_pad_all(_header_container, 0, 0);
    lv_obj_set_style_radius(_header_container, 0, 0); // Disable radius
    lv_obj_remove_flag(_header_container, LV_OBJ_FLAG_SCROLLABLE);

    // 2. Create the scrolling news ticker text box layer
    /*_ticker_label = lv_label_create(_header_container);
    lv_obj_set_style_text_color(_ticker_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(_ticker_label, &lv_font_montserrat_8, 0); // Ultra-compact font
    lv_obj_add_flag(_ticker_label, LV_OBJ_FLAG_HIDDEN);*/ // Hidden initially

    // 1. Create ONLY the parent container
    _bar_chart_container = lv_obj_create(_header_container);
    lv_obj_set_size(_bar_chart_container, 40, 8);
    lv_obj_align(_bar_chart_container, LV_ALIGN_LEFT_MID, 2, 0);
    lv_obj_set_style_bg_color(_bar_chart_container, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(_bar_chart_container, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(_bar_chart_container, 0, 0);
    lv_obj_set_style_pad_all(_bar_chart_container, 0, 0);
    lv_obj_set_style_radius(_bar_chart_container, 0, 0); 
    lv_obj_remove_flag(_bar_chart_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(_bar_chart_container, _bar_chart_draw_cb, LV_EVENT_DRAW_MAIN, this);

    
    // 4. Create Right Sibling Object: Battery Text/Icon Component
    _battery_label = lv_label_create(_header_container);
    lv_obj_align(_battery_label, LV_ALIGN_RIGHT_MID, -2, 0);
    lv_obj_set_style_text_color(_battery_label, lv_color_white(), 0);
    lv_obj_set_style_text_font(_battery_label, &lv_font_montserrat_8, 0);
    lv_obj_set_style_bg_color(_battery_label, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(_battery_label, LV_OPA_COVER, 0);

}


void Header::update(bool is_visible_on_current_screen) {
    if (_header_container == nullptr) return;

    // === FIX: Force the header container to sit atop all sibling menu widgets ===
    //lv_obj_move_foreground(_header_container);

    // Toggle entire visibility loop state based on active stack capability
    if (!is_visible_on_current_screen) {
        lv_obj_add_flag(_header_container, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    lv_obj_remove_flag(_header_container, LV_OBJ_FLAG_HIDDEN);

    // Core sub-component update tick cycles
    update_utilization_bars(_frames_until_update%3==0);//flickering at 60 Hz is obnoxious (30 Hz redraw cycle on LVGL?)
    if(_frames_until_update==0)
    {
      update_battery_status();
      //process_news_ticker();
      _frames_until_update=60;
    }
    _frames_until_update--;

//    lv_obj_invalidate(_header_container); //observe artifacts at top of screen without this --> 40% utilization, plush crash risk
}

void Header::_bar_chart_draw_cb(lv_event_t * e) {
    lv_obj_t * obj = (lv_obj_t *)lv_event_get_target(e); 
    
    // Unpack your class instance from user data to access variables safely
    Header* instance = (Header*)lv_event_get_user_data(e);
    if (!instance) return;

    // Fetch the hardware/software draw buffer target layer
    lv_layer_t * layer = lv_event_get_layer(e);

    int bar_width = 3;
    int gap = 2;
    
    // Acquire the pixel space base bounds of your container
    lv_area_t obj_coords;
    lv_obj_get_coords(obj, &obj_coords);

    // Set up our low-level rectangle styling configurations
    lv_draw_rect_dsc_t rect_dsc;
    lv_draw_rect_dsc_init(&rect_dsc);
    rect_dsc.bg_opa = LV_OPA_COVER;
    rect_dsc.border_width = 0;
    rect_dsc.radius = 0; // Ensures raw pixel block copies without corner processing math

    // Render every single bar sequentially in a single execution pass
    for (int i = 0; i < HEADER_BAR_COUNT; i++) {
        int bar_height = instance->_last_heights[i];
        
        // Dynamically shift color values depending on peak load triggers
        if (bar_height == HEADER_HEIGHT_PX) {
            rect_dsc.bg_color = lv_color_white();
        } else {
            rect_dsc.bg_color = lv_color_hex(0x505050); // Mid grey layout tone
        }

        lv_area_t bar_area;
        // Map X position mapping parameters
        bar_area.x1 = obj_coords.x1 + (i * (bar_width + gap));
        bar_area.x2 = bar_area.x1 + bar_width - 1;
        
        // Map Y parameters anchored flat against the bottom edge of the frame context
        bar_area.y2 = obj_coords.y2;
        bar_area.y1 = obj_coords.y2 - bar_height + 1;

        // Blit the rectangle instantly to screen memory layers
        lv_draw_rect(layer, &rect_dsc, &bar_area);
    }
}


void Header::update_utilization_bars(bool is_reset_max_tracker) {
    uint32_t total_heap = rp2040.getTotalHeap();
    uint32_t free_heap = rp2040.getFreeHeap();
    struct mallinfo mi = mallinfo();

    // Fast float conversion multiplier
    const float height_multiplier = (float)HEADER_HEIGHT_PX * 0.01f;
    bool needs_redraw = false;

    for (int i = 0; i < HEADER_BAR_COUNT; i++) {
        float percentage = 50.0f; 
        
        //Serial.printf("UTILIZATION: %5d, %5d\n",_sensor_suite->core0_frame_us,_sensor_suite->core1_frame_us);
        if (i == 0)      percentage = (100.0f * _sensor_suite->core0_frame_us) / 16666.6f;
        else if (i == 1) percentage = (100.0f * _sensor_suite->core1_frame_us) / 16666.6f;
        else if (i == 2) percentage = _sensor_suite->lvgl_memory_percent;
        else if (i == 3) percentage = _sensor_suite->lvgl_memory_fragmentation;
        else if (i == 4) percentage = free_heap * 100.0f / total_heap;
        else if (i == 5) {
            float largestBlock = (float)mi.keepcost; 
            percentage = 100.0f - (largestBlock * 100.0f / total_heap);
        }

        // Fast height translation math
        int bar_height = (int)(percentage * height_multiplier);
        if (bar_height < 1) bar_height = 1;
        else if (bar_height > HEADER_HEIGHT_PX) bar_height = HEADER_HEIGHT_PX;

        _max_heights[i]=max(_max_heights[i],bar_height);

        // Check if the state actually changed
        if (_last_heights[i] != _max_heights[i] && is_reset_max_tracker) {
            _last_heights[i]  = _max_heights[i]; // Cache new state
            needs_redraw = true;           // Flag that a redraw is required
            _max_heights[i]=0;
        }
    }

    // Only tell LVGL to redraw if data actually moved
    if (needs_redraw) {
        lv_obj_invalidate(_bar_chart_container);
    }
}


/*void Header::update_utilization_bars() {
    uint32_t total_heap = rp2040.getTotalHeap();
    uint32_t free_heap = rp2040.getFreeHeap();
    struct mallinfo mi = mallinfo();

    int bar_width = 3;
    int gap = 2;

    // Use a pre-calculated multiplier to replace slower division: (HEADER_HEIGHT_PX / 100.0f)
    const float height_multiplier = (float)HEADER_HEIGHT_PX * 0.01f;

    for (int i = 0; i < HEADER_BAR_COUNT; i++) {
        float percentage = 50.0f; 
        
        if (i == 0) percentage = _sensor_suite->core0_frame_us * 100.0f / 16666.0f;
        else if (i == 1) percentage = _sensor_suite->core1_frame_us * 100.0f / 16666.0f;
        else if (i == 2) percentage = _sensor_suite->lvgl_memory_percent;
        else if (i == 3) percentage = free_heap * 100.0f / total_heap;
        else if (i == 4) {
            float largestBlock = (float)mi.keepcost; 
            percentage = 100.0f - (largestBlock * 100.0f / total_heap);
        }

        // Fast height translation math
        int bar_height = (int)(percentage * height_multiplier);
        if (bar_height < 1) bar_height = 1;
        else if (bar_height > HEADER_HEIGHT_PX) bar_height = HEADER_HEIGHT_PX;

        // ⚡ CHANGE GUARD: If the value did not change, do nothing!
        if (_last_heights[i] == bar_height) {
            continue; 
        }
        _last_heights[i] = bar_height; // Cache new height state

        lv_obj_t* line_bar = _bars[i];
        // These execution paths only run if the height changes
        lv_obj_set_size(line_bar, bar_width, bar_height); //adds 60% processing overhead
        lv_obj_set_pos(line_bar, i * (bar_width + gap), 8 - bar_height);
        
        lv_color_t target_color = (bar_height == HEADER_HEIGHT_PX) ? lv_color_white() : lv_color_hex(0x808080);
        lv_obj_set_style_bg_color(line_bar, target_color, 0);
    }
}*/



void Header::update_battery_status() {
    float voltage = _sensor_suite->analog.get_vcc();//_sensor_suite->battery_voltage; 
    float temperature_c = _sensor_suite->analog.get_internal_celsius();
    
    // Assign custom character symbols depending on structural voltage ranges
    const char* symbol = " "; // Full Default
    if (voltage < 2.7f) {
        symbol = "E"; // Empty
    }/* else if (voltage < 3.0f) {
        symbol = " "; // Half-full
    }*/

    static char battery_buffer[32];
    // Map string outputs cleanly using the direct grayscale metrics standard format
    snprintf(battery_buffer, sizeof(battery_buffer), "%dC %0.1fV %s", (uint8_t)temperature_c, voltage, symbol);
    lv_label_set_text(_battery_label, battery_buffer);
}

void Header::process_news_ticker() {
    // 1. Interrupt Check: Detect a fresh message from incoming IR buffer streams
    if (!_is_ticker_running && false/*_sensor_suite->has_new_ir_msg*/) {
        _active_msg = "placeholder_ir_message";//_sensor_suite->get_last_ir_msg();
        /*_sensor_suite->has_new_ir_msg = false;*/ // Consume flag loop trace
        
        if (!_active_msg.empty()) {
            lv_label_set_text(_ticker_label, _active_msg.c_str());
            lv_obj_remove_flag(_ticker_label, LV_OBJ_FLAG_HIDDEN);
            
            // Start completely off-screen right (Pico base display standard width width edge boundary)
            _ticker_x_pos = lv_obj_get_width(lv_screen_active());
            lv_obj_set_pos(_ticker_label, _ticker_x_pos, 1);
            
            _is_ticker_running = true;
            _last_ticker_update_ms = millis();
        }
    }

    // 2. Linear Position Animation Step Ticks
    if (_is_ticker_running) {
        uint32_t now = millis();
        if (now - _last_ticker_update_ms >= 16) { // ~60FPS translation velocity
            _last_ticker_update_ms = now;
            _ticker_x_pos -= 1; // Move leftwards 1 pixel per frame pass

            lv_obj_set_pos(_ticker_label, _ticker_x_pos, 1);

            // Compute structural text content width limits dynamically 
            int32_t text_width = lv_obj_get_width(_ticker_label);
            
            // 3. Destructor Check: Vanish once text completely exits the left screen boundaries
            if (_ticker_x_pos < -text_width) {
                lv_obj_add_flag(_ticker_label, LV_OBJ_FLAG_HIDDEN);
                _is_ticker_running = false;
                _active_msg = "";
            }
        }
    }
}