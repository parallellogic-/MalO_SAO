#pragma once

#include "screen.h"

class Game : public Screen {
  private:

  protected:
    lv_obj_t* _overlay_card = nullptr;
    lv_timer_t* _overlay_timer = nullptr;
    lv_obj_t* _game_container;
    
    void _create_popup_overlay(const std::string& text_str);
    void _clear_popup_overlay();
    static void _overlay_timer_cb(lv_timer_t* timer);
  public:
    Game(const std::string& text, lv_group_t* shared_input_group);
    virtual void begin(bool is_enter_from_above,SensorSuite *sensor_suite); //fetch resources from RAM like imagery or IR configuration 
    virtual ScreenAction update(); 
    virtual void end(bool is_leaving_upward);

    lv_key_t touch_to_key(uint8_t touch) override{
        static const lv_key_t touch2key[] = {(lv_key_t)0,(lv_key_t)0,LV_KEY_HOME,LV_KEY_ESC,LV_KEY_ENTER,LV_KEY_PREV,LV_KEY_UP,LV_KEY_NEXT,LV_KEY_LEFT,LV_KEY_DOWN,LV_KEY_RIGHT};
        //unused, hidden, menu, no, yes, CCW, up, CW, left, down, right
        if(touch>=sizeof(touch2key)/sizeof(touch2key[0])) return (lv_key_t)0; return touch2key[touch]; }
};
