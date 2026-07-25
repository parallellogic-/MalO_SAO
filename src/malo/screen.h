#pragma once

#include <string>
#include <vector>
#include "universal_serial_bus_flash.h" //file operations
#include <malloc.h> // Required for mallinfo()
#include <functional>

//WAS ~/Arduino/libraries/lv_conf.h
//IS ~/Arduino/libraries/lvgl/src/lv_conf.h
#define LV_CONF_INCLUDE_SIMPLE
#include <lvgl.h> 

#define HEADER_BAR_COUNT 6
#define HEADER_HEIGHT_PX 7

#define USER_AGREEMENT_TEXT "MalO\nver1.0.0\nNever settle for those awkward feelings of being alone ever again. "\
"MalO is an exciting and interactive experience that will keep you engaged and intrigued. "\
"The anxiety of social situations can be nerve-racking, but after just a few hours of MalO you will soon forget all about those painful emotions of disappointment. "\
"Be part of the new craze that is quickly becoming the next social substitute. "\
"Remember, the more you participate, the more MalO will engage you. Your experience is completely up to you. Absolutely NO ADS. Enjoy!"

#define INFO_TEXT "Software v1.0.0"

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
  SCREEN_SAVER,//screen savers
  MOUNT_USB,
  PAUSE, //turn off led indicator below screen, special action Resume button, and Exit has different behavior
  USER_AGREEMENT,
  IR_RXD,
  IR_TXD,
  VIBRATION_ALERT,
  AUDIO_ALERT,
  INFO
};

struct ScreenAction {
    ScreenActionType type = ScreenActionType::NONE;
    std::shared_ptr<Screen> next_screen = nullptr;
    AnimationFunc led_upper_func = nullptr;
    AnimationFunc led_lower_func = nullptr;
};

const uint8_t lock_bitmap_data[100] = {
    0x00,0x00,0x00,0x00,0xFF,0xFF,0xFF,0xFF,0x00,0x00,
    0x00,0x00,0x00,0xFF,0x00,0x00,0x00,0x00,0xFF,0x00,
    0x00,0x00,0x00,0xFF,0x00,0x00,0x00,0x00,0xFF,0x00,
    0x00,0x00,0x00,0xFF,0x00,0x00,0x00,0x00,0xFF,0x00,
    0x00,0x00,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    0x00,0x00,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    0x00,0x00,0xFF,0xFF,0x00,0x00,0x00,0x00,0xFF,0xFF,
    0x00,0x00,0xFF,0xFF,0x00,0x00,0x00,0x00,0xFF,0xFF,
    0x00,0x00,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    0x00,0x00,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF
};

const lv_image_dsc_t icon_lock_dsc = {
    .header = {
        .cf = LV_COLOR_FORMAT_L8, // Match your display format
        .w = 10,
        .h = 10,
    },
    .data_size = 100,
    .data = lock_bitmap_data
};

// A simple 10x10 representation of a closed padlock (1 byte per pixel for simplicity / L8 format)
const uint8_t unlock_bitmap_data[100] = {
    0x00,0x00,0x00,0x00,0x00,0xFF,0xFF,0xFF,0x00,0x00,
    0x00,0x00,0x00,0x00,0xFF,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0xFF,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0xFF,0x00,0x00,0x00,0x00,
    0x00,0x00,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    0x00,0x00,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    0x00,0x00,0xFF,0xFF,0x00,0x00,0x00,0x00,0xFF,0xFF,
    0x00,0x00,0xFF,0xFF,0x00,0x00,0x00,0x00,0xFF,0xFF,
    0x00,0x00,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,
    0x00,0x00,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF
};

const lv_image_dsc_t icon_unlock_dsc = {
    .header = {
        .cf = LV_COLOR_FORMAT_L8, // Match your display format
        .w = 10,
        .h = 10,
    },
    .data_size = 100,
    .data = unlock_bitmap_data
};

class Screen{
  protected:
    uint32_t _frame_id=0;//how many frames have been rendered to the screen
    std::string _title;
    bool _is_header=true; //if header at top of screen is visible
    //bool _is_allow_achivement_popup=true; //allow achievenement to pop over this screen
    bool _is_menu=false; //when following POP_TO_MENU, stop on this Screen?
    ScreenConfig _screen_config;//flag for tailoring function of this screen 
    SensorSuite* _sensor_suite;
    std::vector<std::shared_ptr<Screen>> _screen_stack; //pointers to lower-level menus
    //ScreenActionType _next_screen_action;
    // FIXED: Changed to weak_ptr to break the circular reference loop.
    // This is a temporary transition slot, NOT a permanent owner link.
    std::weak_ptr<Screen> _next_screen;
    ScreenAction _update_action; //what to return from an update() call
    lv_obj_t* _lv_panel = nullptr;
    lv_group_t* _input_group = nullptr; //unify user inputs into single locale
    //virtual void _on_focus(lv_group_t* input_group)=0;
  public:
    Screen(const std::string& text, lv_group_t* shared_input_group,ScreenConfig screen_config=ScreenConfig::DEFAULT);
    //virtual ~Screen(); //release all memroy, including links to submenus //virtual
    virtual void begin(bool is_enter_from_above,SensorSuite *sensor_suite); //fetch resources from RAM like imagery or IR configuration 
    virtual ScreenAction update(); 
    virtual void end(bool is_leaving_upward);//release RAM resources acquired in begin(), but keep any inter-relationship pointers in place, ex submenus
    bool is_header(){ return _is_header; }//default to show header
    virtual bool is_allow_achivement_popup(){ return true; } //allow achievements to appear on top of this screen (otherwise achievements need to wait for next opportunity to appear)
    bool is_menu(){ return _is_menu; }//user to catch flag when popping back to "POP_TO_MENU" without dynamic casting functionality avaialble
    const std::string& get_title(){ return _title; }
    void add_subscreen(std::shared_ptr<Screen> subscreen){ _screen_stack.push_back(subscreen); };
    ScreenConfig get_screen_config(){ return _screen_config; }
    //led_function get_led_pattern(bool is_top);//return pointer to function to set leds.  if nullptr, defaults to OFF.
    virtual lv_key_t touch_to_key(uint8_t touch){
        static const lv_key_t touch2key[] = {(lv_key_t)0,(lv_key_t)0,LV_KEY_HOME,LV_KEY_ESC,LV_KEY_ENTER,LV_KEY_ESC,LV_KEY_PREV,LV_KEY_ENTER,LV_KEY_LEFT,LV_KEY_NEXT,LV_KEY_RIGHT};
        //unused, hidden, menu, no, yes, CCW, up, CW, left, down, right
        if(touch>=sizeof(touch2key)/sizeof(touch2key[0])) return (lv_key_t)0; return touch2key[touch]; }
};

class MenuScreen : public Screen{
  private:
    uint8_t _saved_menu_index = 0; //last item selected when headed to a lower submenu - will be re-loaded when user pops back up here
    std::vector<lv_obj_t*> _menu_items;
    static lv_style_t _style_main;
    static lv_style_t _style_focused;
    static bool _styles_initialized;
  protected:
    void _init_styles();
    static void _lv_menu_item_event_cb(lv_event_t * e);
    //void _on_focus(lv_group_t* input_group);
    static void _menu_focus_cb(lv_event_t * e);
    static void _menu_event_cb(lv_event_t * e);
    static void _label_icon_draw_cb(lv_event_t * e);
    bool _is_top_menu(){ std::string main_title = "Main"; return main_title==_title; };
    bool _is_pause_menu(){ std::string main_title = "Pause"; return main_title==_title; };
    void _append_menu_item(const std::shared_ptr<Screen>& subscreen,const std::string& title);
  public:
    MenuScreen(const std::string& title, lv_group_t* shared_input_group,ScreenConfig screen_config=ScreenConfig::DEFAULT);
    //virtual ~MenuScreen();
    void begin(bool is_enter_from_above,SensorSuite *sensor_suite) override;
    ScreenAction update() override;
    void end(bool is_leaving_upward) override;
    //led_function get_led_pattern(bool is_top);//return pointer to function to set leds.  defaults to depth-of-menu indication
};

//to hold both the menu generating the event, and the screen target of where the event points to
struct ScreenContext {
    std::shared_ptr<Screen> target_subscreen;
    MenuScreen* host_menu; 
};

class ScreenSaver : public Screen { //display a looping animation
  private:
    //bool _is_locked=false;
    bool _is_title_visible=false;
    SaveState* _save_state=nullptr;
    lv_obj_t* _overlay_card = nullptr;
    bool _is_vibration_alert=false; //if true, then will buzz the motor on begin(from_above) until timeout
    bool _is_audio_alert=false;//if true, will play tune on achievement
    //lv_obj_t* _lv_canvas; //_lv_panel absorbs button pushes, _lv_canvas is the pixel draw buffer
    std::vector<uint8_t> _frame_duration;//how many frames at 60 FPS to show this image on the screen for (1= 16.6 ms, 2=30 ms, 4=60 ms...)
    std::vector<uint8_t> _frame_order;//list of which frames to show in what order (can show the same frame multiple times in one animation).  last value is which index within THIS list to jump to on completion
    //std::string _root_name=nullptr;//base name of the animation being shown.  will append "_%03d.cmp" at end to get image filename, and ".txt" to get config file
    static uint8_t _pixel_list[SCREEN_WIDTH_PX*SCREEN_HEIGHT_PX];
    uint8_t _frame_index=255;//position within _frame_duration list
    uint8_t _frame_order_index=0;//position within the _frame_order list
    uint8_t _frame_elapsed=0;//how many 60 FPS periods have elapsed in the current aniamtion frame
    static void _screensaver_event_cb(lv_event_t * e);
    void _create_unlock_overlay(lv_obj_t* canvas_obj, const std::string& text_str);
  protected:
    //void _on_focus(lv_group_t* input_group) override;
  public:
    ScreenSaver(const std::string& title, lv_group_t* shared_input_group,SaveState* _save_state);//list of files, list of frame timings, frame order.  or enunm for internal config.  of have as config file in flash.
    //~ScreenSaver() override;
    void set_title_visible(bool is_title_visible){
      if(_title=="Champion" || _title=="Favorite Food") _is_title_visible=false;
      else _is_title_visible=is_title_visible; }
    void set_vibration_alert(bool is_vibration_alert){_is_vibration_alert=is_vibration_alert;}
    void set_audio_alert(bool is_audio_alert){_is_audio_alert=is_audio_alert;}
    void begin(bool is_enter_from_above,SensorSuite *sensor_suite) override;
    ScreenAction update() override;
    bool is_allow_achivement_popup() override{ return false; }//try to avoid one achievement overlapping another
    void end(bool is_leaving_upward) override;
    uint8_t _get_current_frame();
    void _update_current_frame();
    bool is_locked(){ return false; if(_save_state==nullptr) return true; return !_save_state->is_unlocked(_title); }
    //void set_locked(bool is_locked){ _is_locked=is_locked; }
};

class Header {
private:
    SensorSuite* _sensor_suite = nullptr;
    
    // LVGL Layout Widget Handles
    lv_obj_t* _header_container = nullptr;
    lv_obj_t* _bar_chart_container = nullptr;
    lv_obj_t* _ticker_label = nullptr;
    lv_obj_t* _battery_label = nullptr;
    lv_obj_t* _bars[HEADER_BAR_COUNT]={};
    uint8_t _last_heights[HEADER_BAR_COUNT]={};
    uint8_t _max_heights[HEADER_BAR_COUNT]={};
    
    // Ticker State Machine variables
    std::string _active_msg = "";
    int32_t _ticker_x_pos = 0;
    bool _is_ticker_running = false;
    uint32_t _last_ticker_update_ms = 0;

    // Helper utilities to build layout geometry blocks
    void update_utilization_bars(bool is_reset_max_tracker);
    void update_battery_status();
    void process_news_ticker();

    static void _bar_chart_draw_cb(lv_event_t * e);

    float _temperature_c=23.0f;
    float _voltage=3.3f;
    uint8_t _frames_until_update=1;//count that counts down until an update to the values in the header is desired

public:
    // Constructor matching your pointer reference system
    Header(SensorSuite* sensor_suite);
    
    // Called once inside ScreenManager to mount our sub-objects
    void begin();
    
    // Executed continuous frame ticks down inside ScreenManager::update
    void update(bool is_visible_on_current_screen);
};

class LongTextScreen : public Screen {
  private:
    std::string _current_text="1\n2\n3\n4\n5\n6\n7\n8\n9\n10\n11\n12\n13\n14\n15\n16\n17\n18\n19\n20\n21\n22\n23\n24\n25\n26\n27\n28\n29\n30\n31\n32\n33\n34\n35\n36\n37\n38\n39\n40\n41\n42\n43\n44\n45\n46\n47\n48\n49\n50\n51\n52\n53\n54\n55\n56\n57\n58\n59\n60\n61\n62\n63\n64\n65\n66\n67\n68\n69\n70\n71\n72\n73\n74\n75\n76\n77\n78\n79\n80\n81\n82\n83\n84\n85\n86\n87\n88\n89\n90\n91\n92\n93\n94\n95\n96\n97\n98\n99\n100";
    bool _text_changed = false;

    // LVGL widgets
    lv_obj_t* _text_label = nullptr;
    lv_obj_t* _back_btn = nullptr;
    lv_obj_t* _btn_label = nullptr;

    // Tracking flags
    bool _scrolled_to_bottom = false;

    // Styles
    static lv_style_t _style_text;
    static lv_style_t _style_btn_normal;
    static lv_style_t _style_btn_focused;
    static bool _styles_initialized;

    void _init_custom_styles();
    static void _back_btn_event_cb(lv_event_t* e);
    static void _scroll_event_cb(lv_event_t* e);

  public:
    LongTextScreen(const std::string& title, lv_group_t* shared_input_group, ScreenConfig screen_config = ScreenConfig::DEFAULT);
    virtual ~LongTextScreen() = default;

    // Set the initial text to parse
    void setText(const std::string& text);

    // Lifecycle hooks
    void begin(bool is_enter_from_above, SensorSuite *sensor_suite) override;
    ScreenAction update() override;
    void end(bool is_leaving_upward) override;
    lv_key_t touch_to_key(uint8_t touch) override{
        static const lv_key_t touch2key[] = {(lv_key_t)0,(lv_key_t)0,LV_KEY_HOME,LV_KEY_ESC,LV_KEY_ENTER,LV_KEY_PREV,LV_KEY_UP,LV_KEY_NEXT,LV_KEY_LEFT,LV_KEY_DOWN,LV_KEY_RIGHT};
        //unused, hidden, menu, no, yes, CCW, up, CW, left, down, right
        if(touch>=sizeof(touch2key)/sizeof(touch2key[0])) return (lv_key_t)0; return touch2key[touch]; }
};

// ---- BoolScreen ----

class BoolScreen : public Screen
{

private:
    static void _init_custom_styles();

    static void _switch_event_cb(lv_event_t* e);
    static void _back_btn_event_cb(lv_event_t* e);

    // -------- LVGL Objects --------
    lv_obj_t* _switch = nullptr;
    lv_obj_t* _text_label = nullptr;

    lv_obj_t* _back_btn = nullptr;
    lv_obj_t* _btn_label = nullptr;

    // -------- Data --------
    bool _value = false;
    std::string _description;

    std::function<void(bool)> _on_value_changed;

    // -------- Shared Styles --------
    static lv_style_t _style_text;
    static lv_style_t _style_btn_normal;
    static lv_style_t _style_btn_focused;
    static bool _styles_initialized;
public:
    BoolScreen(const std::string& title,
               lv_group_t* shared_input_group,
               ScreenConfig screen_config);

    void begin(bool is_enter_from_above,
               SensorSuite* sensor_suite) override;

    ScreenAction update() override
    {
        return Screen::update();
    }

    void end(bool is_leaving_upward) override;

    void setText(const std::string& text);

    void setValue(bool value);
    bool getValue() const;

    void setValueChangedCallback(std::function<void(bool)> callback)
    {
        _on_value_changed = std::move(callback);
    }
};