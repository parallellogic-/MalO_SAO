#include "quiz.h"

Quiz::Quiz(const std::string& text, lv_group_t* shared_input_group) 
    : Game(text, shared_input_group) { // Forward initialization arguments to Game constructor
    
    _is_menu = false; 
    
    _questions = {
        {
            "An unknown app named 'MalO ver1.0.0' appears on your device. Open it?",
            {"Install immediately", "Ignore it", "Try to delete it"},
            "" 
        },
        {
            "Images arrive showing a skull-headed entity inside your home. What do you leave out for her?",
            {"Sandwich", "Lasagna", "Copper wires"},
            "Favorite Food" 
        },
        {
            "An encrypted terminal feed opens: Who shall I follow home tonight?",
            {"User", "SCP Guard", "O5 Council"},
            "" 
        },
        {
            "You scold MalO for breaching containment.  She looks up with oversized digital eyes.",
            {
                "Patch the firewall",
                "Sigh and forgive her", 
                "Boop her snout"
            },
            "Innocent"
        },
        {
            "The application requests root access. Do you authorize it?",
            {
                "Force safe mode", 
                "Overwrite bootloader",
                "Grant total control"
            },
            "Evil"
        },
        {
            "The entity is now standing directly behind you.  How do you respond?",
            {"Freeze in place", "Recoil in fear", "Take selfie together"},
            ""
        },
        {
            "MalO has integrated into your life completely. Do you accept your new companion?",
            {"Yes!", "Definitely!", "Absolutely!"},
            "Know MalO" 
        }
    };
}

void Quiz::begin(bool is_enter_from_above, SensorSuite *sensor_suite) {
    _sensor_suite = sensor_suite;
    
    // Clear out navigation action states up front
    _update_action.type = ScreenActionType::NONE;
    _next_screen.reset(); 

    if (is_enter_from_above) {
        _current_question_index = 0; //  Reset quiz state only when entering fresh from above
        _overlay_card = nullptr;
        _overlay_timer = nullptr;

        // Allocate the baseline panel container matching screen specs
        _lv_panel = lv_obj_create(lv_screen_active());
        lv_obj_set_size(_lv_panel, SCREEN_WIDTH_PX, SCREEN_HEIGHT_PX);
        
        // Tight layout: Stack elements using a flex column
        lv_obj_set_flex_flow(_lv_panel, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(_lv_panel, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        
        // SQUEEZE: Force the space between stacked items (gap) to be tight (only 3 pixels)
        lv_obj_set_style_pad_row(_lv_panel, 3, LV_PART_MAIN);
        
        lv_obj_set_style_bg_color(_lv_panel, lv_color_black(), LV_PART_MAIN);
        lv_obj_set_style_border_width(_lv_panel, 0, LV_PART_MAIN);
        
        // SQUEEZE: Tight outer margins on the overall screen space
        lv_obj_set_style_pad_left(_lv_panel, 6, LV_PART_MAIN);
        lv_obj_set_style_pad_right(_lv_panel, 6, LV_PART_MAIN);
        lv_obj_set_style_pad_top(_lv_panel, 4, LV_PART_MAIN);
        lv_obj_set_style_pad_bottom(_lv_panel, 4, LV_PART_MAIN);
        lv_obj_set_scrollbar_mode(_lv_panel, LV_SCROLLBAR_MODE_OFF);
        
        if (is_header()) {
            lv_obj_set_style_pad_top(_lv_panel, HEADER_HEIGHT_PX + 4, LV_PART_MAIN);
        }

        // Initialize question prompt text layer
        _prompt_label = lv_label_create(_lv_panel);
        lv_obj_set_width(_prompt_label, lv_pct(100));
        lv_obj_set_height(_prompt_label, LV_SIZE_CONTENT);
        lv_label_set_long_mode(_prompt_label, LV_LABEL_LONG_WRAP);
        
        lv_obj_set_style_text_font(_prompt_label, &lv_font_montserrat_10, LV_PART_MAIN); 
        lv_obj_set_style_text_color(_prompt_label, lv_color_hex(0x00FF00), LV_PART_MAIN); 
        lv_obj_set_style_text_align(_prompt_label, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
        
        // SQUEEZE: Drop bottom margin of prompt to give the buttons breathing room
        lv_obj_set_style_margin_bottom(_prompt_label, 4, LV_PART_MAIN);

        // Generate options arrays 
        _load_current_question();
        
        // Clear hidden state flags cleanly
        lv_obj_clear_flag(_lv_panel, LV_OBJ_FLAG_HIDDEN);
    } else {
        //  Returning from below (e.g. out of an unrolled cutscene submenu): unhide base canvas
        lv_obj_clear_flag(_lv_panel, LV_OBJ_FLAG_HIDDEN);
        
        // Ensure options tracking pointers are re-attached to the active input group safely
        if (_input_group && !_option_buttons.empty()) {
            for (lv_obj_t* btn : _option_buttons) {
                lv_group_add_obj(_input_group, btn);
            }
            lv_group_focus_obj(_option_buttons.front());
        }
    }

    // Force canvas invalidation update passes
    if (_lv_panel) {
        lv_obj_invalidate(_lv_panel);
    }
}



void Quiz::_load_current_question() {
    if (_current_question_index >= _questions.size()) return;

    _clean_current_options();
    
    const auto& q = _questions[_current_question_index];
    lv_label_set_text(_prompt_label, q.prompt.c_str());

    for (size_t i = 0; i < q.options.size(); ++i) {
        lv_obj_t* btn = lv_button_create(_lv_panel);
        lv_obj_set_width(btn, lv_pct(98)); 
        lv_obj_set_height(btn, LV_SIZE_CONTENT);
        
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x111111), LV_PART_MAIN);
        lv_obj_set_style_border_width(btn, 1, LV_PART_MAIN);
        lv_obj_set_style_border_color(btn, lv_color_hex(0x333333), LV_PART_MAIN);
        
        // SQUEEZE: Aggressively flatten vertical padding inside the buttons
        lv_obj_set_style_pad_top(btn, 2, LV_PART_MAIN);
        lv_obj_set_style_pad_bottom(btn, 2, LV_PART_MAIN);
        lv_obj_set_style_pad_left(btn, 4, LV_PART_MAIN);
        lv_obj_set_style_pad_right(btn, 4, LV_PART_MAIN);
        
        // SQUEEZE: Zero out external default margins that push buttons apart
        lv_obj_set_style_margin_all(btn, 0, LV_PART_MAIN);
        
        lv_obj_set_style_border_color(btn, lv_color_hex(0x00FF00), LV_STATE_FOCUSED);
        lv_obj_set_style_bg_color(btn, lv_color_hex(0x224422), LV_STATE_FOCUSED);

        lv_obj_remove_flag(btn, LV_OBJ_FLAG_SCROLL_ON_FOCUS);
        lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
        
        lv_obj_set_user_data(btn, this);
        
        lv_obj_t* label = lv_label_create(btn);
        lv_label_set_text(label, q.options[i].c_str());
        lv_obj_center(label);
        
        lv_obj_set_style_text_font(label, &lv_font_montserrat_10, LV_PART_MAIN);
        lv_obj_set_style_text_color(label, lv_color_white(), LV_PART_MAIN);

        // SQUEEZE: Eliminate labels forcing extra sizing bounds
        lv_obj_set_style_margin_all(label, 0, LV_PART_MAIN);
        lv_obj_set_style_pad_all(label, 0, LV_PART_MAIN);

        lv_obj_add_event_cb(btn, Quiz::_option_click_cb, LV_EVENT_CLICKED, nullptr);

        if (_input_group) { // Updated variable name
            lv_group_add_obj(_input_group, btn);
        }

        _option_buttons.push_back(btn);
    }

    if (!_option_buttons.empty() && _input_group) { // Updated variable name
        lv_group_focus_obj(_option_buttons.front());
    }
}



void Quiz::_clean_current_options() {
    if (_input_group) { // Updated to your exact private variable name
        for (lv_obj_t* btn : _option_buttons) {
            lv_group_remove_obj(btn);
            lv_obj_delete(btn);
        }
    } else {
        for (lv_obj_t* btn : _option_buttons) {
            lv_obj_delete(btn);
        }
    }
    _option_buttons.clear();
}

void Quiz::_option_click_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    
    // Check if the event is a targeted keypad event
    if (code == LV_EVENT_KEY) {
        uint32_t key = lv_event_get_key(e);
        
        // If the user pressed any button other than the selection confirmation key,
        // let LVGL handle group navigation naturally (moving up/down selection).
        if (key != LV_KEY_ENTER) {
            return; 
        }
    }

    // Execution lands here on a valid click or an input group ENTER keypress confirmation
    lv_obj_t* btn = (lv_obj_t*)lv_event_get_target(e);
    Quiz* instance = (Quiz*)lv_obj_get_user_data(btn);
    
    if (!instance) return;

    const auto& current_q = instance->_questions[instance->_current_question_index];

    // Process sensor unlocks
    if (!current_q.unlock_key.empty() && instance->_sensor_suite) {
        instance->_sensor_suite->save_state.unlock(current_q.unlock_key);
    }

    instance->_current_question_index++;

    if (instance->_current_question_index >= instance->_questions.size()) {
        instance->_update_action.type = ScreenActionType::POP_BACK;
        instance->_update_action.next_screen = nullptr;
    } else {
        instance->_load_current_question();
    }
}

ScreenAction Quiz::update() {
    // Relying strictly on the callback loop for button updates.
    // Retaining pure pass-through return layer.
    return _update_action;
}

void Quiz::end(bool is_leaving_upward) {
    if(is_leaving_upward)
    {
      _clean_current_options();

      if (_prompt_label) {
          lv_obj_delete(_prompt_label);
          _prompt_label = nullptr;
      }

      if (_lv_panel) {
          lv_obj_delete(_lv_panel);
          _lv_panel = nullptr;
      }
    }else{

            lv_obj_add_flag(_lv_panel, LV_OBJ_FLAG_HIDDEN);
    }
    
    // Explicit call to clear base Game overlay assets
    _clear_popup_overlay(); 
}
