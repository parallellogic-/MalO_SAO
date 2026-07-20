#pragma once

#include "game.h" // Inherit from Game instead of Screen
#include <vector>
#include <string>

struct QuizQuestion {
    std::string prompt;
    std::vector<std::string> options;
    std::string unlock_key; 
};

class Quiz : public Game {
  private:
    std::vector<QuizQuestion> _questions;
    size_t _current_question_index = 0;
    
    lv_obj_t* _prompt_label = nullptr;
    std::vector<lv_obj_t*> _option_buttons;
    
    void _load_current_question();
    void _clean_current_options();
    static void _option_click_cb(lv_event_t* e);

  public:
    Quiz(const std::string& text, lv_group_t* shared_input_group);
    virtual void begin(bool is_enter_from_above, SensorSuite *sensor_suite) override;
    virtual ScreenAction update() override; 
    virtual void end(bool is_leaving_upward) override;

    lv_key_t touch_to_key(uint8_t touch) override {
    static const lv_key_t touch2key[] = {
        (lv_key_t)0, (lv_key_t)0,  LV_KEY_HOME, LV_KEY_ESC, LV_KEY_ENTER, 
        LV_KEY_PREV, LV_KEY_PREV,  LV_KEY_NEXT, LV_KEY_LEFT, LV_KEY_NEXT, LV_KEY_RIGHT
    };

    if(touch >= sizeof(touch2key)/sizeof(touch2key[0])) return (lv_key_t)0; 
    return touch2key[touch]; 
}
};
