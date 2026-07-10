#include "screen.h"

lv_style_t MenuScreen::_style_main;
lv_style_t MenuScreen::_style_focused;
bool MenuScreen::_styles_initialized = false;

Screen::Screen(const std::string& title, lv_group_t* shared_input_group): _title(title), _input_group(shared_input_group) {
  Serial.printf("Screen START\n"); delay(10);
}

void Screen::begin(bool is_enter_from_above)
{
  Serial.printf("Screen::begin called\n");
  if (_lv_panel)
  {
    lv_obj_remove_flag(_lv_panel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clean(_lv_panel);
  }
  if(_input_group)
  {
    lv_group_remove_all_objs(_input_group);
    _on_focus(_input_group); 
  }
}

ScreenAction Screen::update()
{
  return { ScreenActionType::NONE }; // Stay on this screen
}

void Screen::end(bool is_leaving_upward)
{
  if (_lv_panel) lv_obj_add_flag(_lv_panel, LV_OBJ_FLAG_HIDDEN);
}

//void Screen::_on_focus(lv_group_t* input_group){}
//void Screen::_handle_button(uint32_t key, bool pressed){}

// ---- Menu ----

MenuScreen::MenuScreen(const std::string& title, lv_group_t* shared_input_group): Screen(title,shared_input_group)
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

void MenuScreen::begin(bool is_enter_from_above)
{
  Serial.printf("MenuScreen.begin called %d\n",is_enter_from_above);
  Screen::begin(is_enter_from_above);

  if(is_enter_from_above)
  {
    _menu_items.clear(); // Wipe out pointers from previous allocations

    // 2. Loop through child pointers and dynamically instantiate UI elements
    for (const std::shared_ptr<Screen>& subscreen : _screen_stack) {
        lv_obj_t * lbl = lv_label_create(_lv_panel); 
        lv_label_set_text(lbl, subscreen->get_title().c_str());
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

    if(!_is_top_menu())
    {//add back button if there is somewhere up the user can go

    }

    // Focus the initial topmost list option
    if (lv_obj_get_child_cnt(_lv_panel) > 0 && _input_group) {
        lv_group_focus_obj(lv_obj_get_child(_lv_panel, 0));
    }

    _on_focus(_input_group);
  }
}

ScreenAction MenuScreen::update()
{
    /*if (user_selected_brightness) {
      // Instantiate the submenu
      auto sub = std::make_shared<BrightnessScreen>(); 
      return { ScreenAction::PUSH_SUBMENU, sub };
  }*/

  //POP_BACK
  ScreenAction action;
  action.type=_next_screen_action;
  _next_screen_action=ScreenActionType::NONE;
  if(action.type==ScreenActionType::PUSH_SUBMENU)
  {
      action.next_screen=_next_screen.lock();
      _next_screen.reset();//release pointer
  }
  return action; // Stay on this screen
}

void MenuScreen::end(bool is_leaving_upward)
{
    if(is_leaving_upward)
    {
      _menu_items.clear(); 
    }
    Screen::end(is_leaving_upward);
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

            // Use your screens here safely!
        
            if (target_screen && parent_menu) {
                //parent_menu->handle_selection(target_screen);
                parent_menu->_next_screen_action=ScreenActionType::PUSH_SUBMENU;
                parent_menu->_next_screen=target_screen;
            }
        }
    }
}

void MenuScreen::_handle_button(uint32_t key, bool pressed) {
    //if (!pressed) return; // Only trigger action on release/press down

    /*switch(key) {
        case LV_KEY_NEXT:
            // Move selection highlight down
            break;
        case LV_KEY_ENTER:
            // Click the selected menu item or open subscreen
            break;
    }*/
}

void MenuScreen::_on_focus(lv_group_t* input_group) {
    lv_group_remove_all_objs(input_group); // Clear old screen focus
    for (auto* item : _menu_items) {
        if(item!=nullptr) lv_group_add_obj(input_group, item); // Add this menu's buttons
    }
}

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
    MenuScreen* instance = (MenuScreen*)lv_display_get_user_data(NULL);
    if (!instance) return;

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
            if(!instance->_is_top_menu()) instance->_next_screen_action=ScreenActionType::POP_BACK;
            return;
        }
        
        if (key == LV_KEY_HOME) {
            // Direct escape straight to the root menu frame from any depth layer
            /*if (instance->_active_menu != instance->_menu_main) {
                //instance->switch_menu(instance->_menu_main, true);
                //instance->led_cb(true); //turn off leds if leaving the Animations menu
                
            }*/
            if(!instance->_is_top_menu()) instance->_next_screen_action=ScreenActionType::POP_TO_TOP;
            return;
        }
    }

    // ==========================================
    // 2. ITEM CLICK MANAGEMENT (FORWARDS)
    // ==========================================
    /*if (code == LV_EVENT_CLICKED) {
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
            //for(int iter=0;iter<500;iter++) instance->_level_buffer[iter]=0xFF;//temp display
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
        else if (strcmp(text, "Mount USB") == 0) {
            //TODO: show graphic of USB symbol = mounted
            UniversalSerialBus::set_mounted();
        }
        // Unified Back string tracker handles older menu formats seamlessly
        else if (strcmp(text, "Back") == 0) {
            lv_obj_t * parent_menu = (lv_obj_t*)lv_obj_get_user_data(obj);
            if (parent_menu) instance->switch_menu(parent_menu, true);
        }
    }
    instance->led_cb(true); //turn off leds if leaving the Animations menu */
}


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

