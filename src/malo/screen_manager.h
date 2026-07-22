#pragma once
#include <vector>
#include <cstdint>
#include "malo.h"
#include "oled.h"
#include "led.h"
#include <memory>
#include "screen.h"
#include "tictactoe.h"
#include "pong.h"
#include "snake.h"
#include "labyrinth.h"
#include "pipe.h"
#include "lights_out.h"
#include "quiz.h"

// Forward declaration of LVGL object type to avoid including lvgl.h in the header
//struct _lv_obj_t;
//typedef struct _lv_obj_t lv_obj_t;

class AchivementState{
  private:
    uint32_t _start_ms=0;
    uint32_t _min_ms=0;
    bool _was_sustained=false;
  public:
    AchivementState(uint32_t min_ms):_min_ms(min_ms){};
    bool is_sustained(bool is_valid){
        if(is_valid)
        {
          if(_start_ms==0) _start_ms=millis();
          else if(millis()-_start_ms>_min_ms){ _was_sustained=true; return true; }
        }else _start_ms=0;
        return false;
    }
    bool was_sustained(){ return _was_sustained; }//monitor this if need to wait for is_sustained to clear before proceeding
};

class AchievementManager{
  private:
    SensorSuite* _sensor_suite;
    AchivementState booper=AchivementState(70);
    AchivementState hall=AchivementState(1000);
    AchivementState music=AchivementState(250);
    AchivementState undervolt=AchivementState(1000);
    AchivementState sunny=AchivementState(1000);
    AchivementState hot=AchivementState(1000);
    AchivementState cold=AchivementState(1000);
    AchivementState dizzy=AchivementState(300);
    AchivementState bored_0=AchivementState(5*60*1000);
    AchivementState bored_1=AchivementState(15*60*1000);
    float potentiometer=0;
  public:
    AchievementManager(SensorSuite* sensor_suite):_sensor_suite(sensor_suite){}
    void begin();
    void update();
};

class ScreenManager {
private:
  // -- demo setup --
    bool _is_demo_mode = false;
    size_t _demo_step_index = 0;
    uint32_t _demo_step_start_ms = 0;
    enum class DemoStepType { SCREENSAVER, STATIC_TEXT };
    struct DemoStep {
        DemoStepType type;
        std::string payload; // Title string for screensavers, text string for static screen
        uint32_t duration_ms;
    };
    std::vector<DemoStep> _demo_routine;
    lv_obj_t* _demo_text_screen = nullptr;
    lv_obj_t* _demo_text_label = nullptr;
    void _init_demo_mode();
    void _handle_demo_mode();
    void _show_static_text_screen(const std::string& text);
    void _hide_static_text_screen();

    // -- standard config --
    SensorSuite* _sensor_suite;
    uint8_t _canvas_buffer[SCREEN_WIDTH_PX * SCREEN_HEIGHT_PX] __attribute__((aligned(4)));
    //uint8_t _screen_buffer[SCREEN_WIDTH_PX * SCREEN_HEIGHT_PX] __attribute__((aligned(4)));
    uint32_t _last_update_ms=0;

    std::unique_ptr<Header> _system_header;
    std::unique_ptr<AchievementManager> _achievement_manager;

    std::vector<std::shared_ptr<Screen>> _screen_stack; //trace of the menus from the top to where the user currently is (dyanmically changes based on user interaction)
    std::vector<std::shared_ptr<ScreenSaver>> _screen_savers; //only init list once, so track here so it can be re-used to save memory
    //std::shared_ptr<Screen> _active_screen = nullptr;
    lv_obj_t* _screen_canvas = nullptr;
    lv_obj_t* _header = nullptr;
    lv_group_t* _shared_input_group = nullptr; 
    uint8_t _last_raw_button=0;
    std::string _ir_rxd_str = ""; //this is a list of the recent IR messages received
    std::shared_ptr<LongTextScreen> _ir_rxd_screen=nullptr;
    //bool _ir_rxd_ping_pong=false;

    //animations - when in aniamtions menu, track which aniamtions are shown
    AnimationFunc _led_upper_func = nullptr;
    AnimationFunc _led_lower_func = nullptr;
    
    static void _display_flush_cb(lv_display_t * disp, const lv_area_t * area, uint8_t * px_map);
    static void _button_read_cb(lv_indev_t * indev, lv_indev_data_t * data);
    void _lvgl2spi(uint8_t* src,OLED &oled);
    void _pop_screen();
    void _push_screen(std::shared_ptr<Screen> new_screen);
    std::shared_ptr<Screen> _get_active_screen(){ if (_screen_stack.empty()) return nullptr; return _screen_stack.back(); }
    void _set_menu_structure();
    void _update_ir_rxd();//fetch any strings that came in, update the internal state tracker to show the full list of recent messages
public:
  ScreenManager();
  void begin(SensorSuite &sensor_suite);
  void update();
  void end();
  void diag();
  int8_t get_screen_stack_depth(){ return _screen_stack.size(); }
  std::string get_ir_rxd_text(){ if(_ir_rxd_str.length()==0) return "No messages received yet"; return _ir_rxd_str; }
};
