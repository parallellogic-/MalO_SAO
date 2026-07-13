#pragma once

#include "screen.h"

class Game : public Screen{
  private:
  protected:
  public:
    Game(const std::string& text, lv_group_t* shared_input_group);
    virtual void begin(bool is_enter_from_above); //fetch resources from RAM like imagery or IR configuration 
    virtual ScreenAction update(); 
    virtual void end(bool is_leaving_upward);
};
