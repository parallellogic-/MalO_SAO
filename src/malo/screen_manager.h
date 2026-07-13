#pragma once
#include <vector>
#include <cstdint>
#include "malo.h"
#include "oled.h"
#include "led.h"
#include <memory>
#include "screen.h"
#include "tictactoe.h"


// Forward declaration of LVGL object type to avoid including lvgl.h in the header
//struct _lv_obj_t;
//typedef struct _lv_obj_t lv_obj_t;


class ScreenManager {
private:
    SensorSuite* _sensor_suite;
    uint8_t _canvas_buffer[SCREEN_WIDTH_PX * SCREEN_HEIGHT_PX] __attribute__((aligned(4)));
    uint8_t _screen_buffer[SCREEN_WIDTH_PX * SCREEN_HEIGHT_PX] __attribute__((aligned(4)));
    uint32_t _last_update_ms=0;

    std::vector<std::shared_ptr<Screen>> _screen_stack; //trace of the menus from the top to where the user currently is (dyanmically changes based on user interaction)
    //std::shared_ptr<Screen> _active_screen = nullptr;
    lv_obj_t* _screen_canvas = nullptr;
    lv_obj_t* _header = nullptr;
    lv_group_t* _shared_input_group = nullptr; 
    uint8_t _last_raw_button=0;

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
public:
  ScreenManager();
  void begin(SensorSuite &sensor_suite);
  void update();
  void end();
  void diag();
  int8_t get_screen_stack_depth(){ return _screen_stack.size(); }
};
