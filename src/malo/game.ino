#include "game.h"

Game::Game(const std::string& text, lv_group_t* shared_input_group) : Screen(text, shared_input_group) {}

void Game::begin(bool is_enter_from_above,SensorSuite *sensor_suite) 
{
    // Chain up to the base Screen class so it handles visibility/layouts
    Screen::begin(is_enter_from_above,sensor_suite); 
}


ScreenAction Game::update() 
{
    // Chain up to the base Screen class so it handles visibility/layouts
    _frame_id++;
    return _update_action;
}

void Game::end(bool is_leaving_upward) 
{
    // Chain up to the base Screen class to handle tear-down
    Screen::end(is_leaving_upward);
}

void Game::_overlay_timer_cb(lv_timer_t* timer) {
    Game* instance = (Game*)lv_timer_get_user_data(timer);
    if (instance) {
        instance->_clear_popup_overlay();
    }
}

// Safely deletes the active popup card and resets tracking structures
void Game::_clear_popup_overlay() {
    if (_overlay_timer) {
        lv_timer_delete(_overlay_timer);
        _overlay_timer = nullptr;
    }
    if (_overlay_card) {
        lv_obj_delete(_overlay_card);
        _overlay_card = nullptr;
    }
}

// Spawns a standardized temporary system dialog over the active board area
void Game::_create_popup_overlay(const std::string& text_str) {
    // Wipe out old overlay structures cleanly if they exist
    _clear_popup_overlay();

    if (_game_container == nullptr) return;

    // Build the structural container over the game framework
    _overlay_card = lv_obj_create(_game_container);
    //lv_obj_move_foreground(_overlay_card);
    
    // Geometry bounds targeting a sleek center banner layout
    lv_obj_set_size(_overlay_card, 115, 26);                   
    lv_obj_align(_overlay_card, LV_ALIGN_CENTER, 0, 0);    
    
    lv_obj_set_style_pad_all(_overlay_card, 4, 0);
    lv_obj_set_style_pad_gap(_overlay_card, 6, 0);             
    lv_obj_set_flex_flow(_overlay_card, LV_FLEX_FLOW_ROW);      
    lv_obj_set_flex_align(_overlay_card, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_scrollbar_mode(_overlay_card, LV_SCROLLBAR_MODE_OFF);

    // Dark visual styling matching screensaver assets
    lv_obj_set_style_bg_color(_overlay_card, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(_overlay_card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(_overlay_card, 0, 0);        
    lv_obj_set_style_outline_color(_overlay_card, lv_color_white(), 0);
    lv_obj_set_style_outline_width(_overlay_card, 1, 0);       
    lv_obj_set_style_radius(_overlay_card, 4, 0);

    // Dynamic descriptive string asset management
    lv_obj_t* label = lv_label_create(_overlay_card);
    lv_label_set_text(label, text_str.c_str());
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    
    if (text_str.length() > 15) {
        lv_obj_set_style_text_font(label, &lv_font_montserrat_8, 0); 
    } else {
        lv_obj_set_style_text_font(label, &lv_font_montserrat_10, 0); 
    }

    // Register a thread-safe system timer to dismiss after 2000 milliseconds
    _overlay_timer = lv_timer_create(_overlay_timer_cb, 2000, this);
}