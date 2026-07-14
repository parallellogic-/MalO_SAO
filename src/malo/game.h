#pragma once

#include "screen.h"

class Game : public Screen {
  private:
  protected:
  public:
    Game(const std::string& text, lv_group_t* shared_input_group);
    virtual void begin(bool is_enter_from_above); //fetch resources from RAM like imagery or IR configuration 
    virtual ScreenAction update(); 
    virtual void end(bool is_leaving_upward);

    lv_key_t touch_to_key(uint8_t touch) override{
        static const lv_key_t touch2key[] = {(lv_key_t)0,(lv_key_t)0,LV_KEY_HOME,LV_KEY_ESC,LV_KEY_ENTER,LV_KEY_PREV,LV_KEY_UP,LV_KEY_NEXT,LV_KEY_LEFT,LV_KEY_DOWN,LV_KEY_RIGHT};
        //unused, hidden, menu, no, yes, CCW, up, CW, left, down, right
        if(touch>=sizeof(touch2key)/sizeof(touch2key[0])) return (lv_key_t)0; return touch2key[touch]; }
};
