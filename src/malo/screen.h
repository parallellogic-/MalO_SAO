#pragma once

#include <string>
#include <vector>

//WAS ~/Arduino/libraries/lv_conf.h
//IS ~/Arduino/libraries/lvgl/src/lv_conf.h
#define LV_CONF_INCLUDE_SIMPLE
#include <lvgl.h> 

class Screen;

enum class ScreenActionType : uint8_t { 
    NONE, 
    PUSH_SUBMENU, 
    POP_BACK,
    POP_TO_MENU,
    POP_TO_TOP
};

enum class MenuItem : uint8_t{ //button labels with special functions
  STANDARD,
  BACK,//up one menu level
  RESUME,//up one menu level (back into game)
  EXIT//up mulitple leves
};

enum class ScreenConfig{
  DEFAULT, //top-level items pointing to lower-level screens
  ANIMATIONS,//if in this menu or one lower, will show the led and screen saver selected (persistent)
  LED_UPPER, //selecting menu items will change LED state
  LED_LOWER,
  SCREEN_SAVER//screen savers
};

struct ScreenAction {
    ScreenActionType type = ScreenActionType::NONE;
    std::shared_ptr<Screen> next_screen = nullptr;
    AnimationFunc led_upper_func = nullptr;
    AnimationFunc led_lower_func = nullptr;
};

class Screen{
  protected:
    std::string _title;
    bool _is_header=true; //if header at top of screen is visible
    bool _is_allow_interruption=true; //allow achievenement to pop over this screen
    bool _is_menu=false; //when following POP_TO_MENU, stop on this Screen?
    ScreenConfig _screen_config;//flag for tailoring function of this screen 
    std::vector<std::shared_ptr<Screen>> _screen_stack; //pointers to lower-level menus
    //ScreenActionType _next_screen_action;
    // FIXED: Changed to weak_ptr to break the circular reference loop.
    // This is a temporary transition slot, NOT a permanent owner link.
    std::weak_ptr<Screen> _next_screen;
    ScreenAction _update_action; //what to return from an update() call
    lv_obj_t* _lv_panel = nullptr;
    lv_group_t* _input_group = nullptr; //unify user inputs into single locale
    virtual void _on_focus(lv_group_t* input_group)=0;
    virtual void _handle_button(uint32_t key, bool pressed)=0;
  public:
    Screen(const std::string& text, lv_group_t* shared_input_group,ScreenConfig screen_config=ScreenConfig::DEFAULT);
    ~Screen() = default; //release all memroy, including links to submenus //virtual
    virtual void begin(bool is_enter_from_above); //fetch resources from RAM like imagery or IR configuration 
    virtual ScreenAction update(); 
    virtual void end(bool is_leaving_upward);//release RAM resources acquired in begin(), but keep any inter-relationship pointers in place, ex submenus
    bool is_header(){ return _is_header; }//default to show header
    bool is_allow_interruption(){ return _is_allow_interruption; } //allow achievements to appear on top of this screen (otherwise achievements need to wait for next opportunity to appear)
    bool is_menu(){ return _is_menu; }//user to catch flag when popping back to "POP_TO_MENU" without dynamic casting functionality avaialble
    const std::string& get_title(){ return _title; }
    void add_subscreen(std::shared_ptr<Screen> subscreen){ _screen_stack.push_back(subscreen); };
    //led_function get_led_pattern(bool is_top);//return pointer to function to set leds.  if nullptr, defaults to OFF.
};

class MenuScreen : public Screen{
  private:
    std::vector<lv_obj_t*> _menu_items;
    static lv_style_t _style_main;
    static lv_style_t _style_focused;
    static bool _styles_initialized;
  protected:
    void _init_styles();
    static void _lv_menu_item_event_cb(lv_event_t * e);
    void _on_focus(lv_group_t* input_group);
    void _handle_button(uint32_t key, bool pressed);
    static void _menu_focus_cb(lv_event_t * e);
    static void _menu_event_cb(lv_event_t * e);
    bool _is_top_menu(){ std::string main_title = "Main"; return main_title==_title; };
    bool _is_pause_menu(){ std::string main_title = "Pause"; return main_title==_title; };
    void _append_menu_item(const std::shared_ptr<Screen>& subscreen,const std::string& title);
  public:
    MenuScreen(const std::string& title, lv_group_t* shared_input_group,ScreenConfig screen_config=ScreenConfig::DEFAULT);
    virtual ~MenuScreen();
    void begin(bool is_enter_from_above) override;
    ScreenAction update() override;
    void end(bool is_leaving_upward) override;
    //led_function get_led_pattern(bool is_top);//return pointer to function to set leds.  defaults to depth-of-menu indication
};

//to hold both the menu generating the event, and the screen target of where the event points to
struct ScreenContext {
    std::shared_ptr<Screen> target_subscreen;
    MenuScreen* host_menu; 
};

class ScreenSaver : public Screen{ //display a looping animation
  private:

  public:
    ScreenSaver()//list of files, list of frame timings, frame order
}

/*class Header{

};

class AchievementScreen{
  private:

  public:
    AchievementScreen();
};

class LevelScreen{

};*/