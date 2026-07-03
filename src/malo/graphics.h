#pragma once

#include "malo.h"
#include "screen.h"
#include "led.h"
//#include "level.h"

//WAS ~/Arduino/libraries/lv_conf.h
//IS ~/Arduino/libraries/lvgl/src/lv_conf.h
#define LV_CONF_INCLUDE_SIMPLE
#include <lvgl.h>

// Forward declaration tells the compiler this struct exists
struct SensorSuite; 

class Graphics{

  private:
    SensorSuite* _sensor_suite = nullptr;
    uint32_t _last_update_ms=0;
    uint8_t _last_raw_button=0;
    lv_group_t * _input_group = nullptr;    // Kept as a member to load/unload submenus
    lv_obj_t * _active_menu = nullptr;      // Points to the currently visible container
    lv_obj_t * _menu_main = nullptr;        // Root main menu panel
    lv_obj_t * _menu_settings = nullptr;    // Settings submenu container
    lv_obj_t * _menu_animations = nullptr;  // Animations submenu container
    lv_obj_t * _menu_animations_upper_leds = nullptr;  // Animations submenu container
    lv_obj_t * _menu_animations_lower_leds = nullptr;  // Animations submenu container
    lv_obj_t * _menu_animations_screen = nullptr;  // Animations submenu container
    lv_obj_t * _menu_levels = nullptr; //games/puzzles
    lv_obj_t * _menu_messages = nullptr; //ir receive and transmit
    lv_obj_t * _menu_periphreal_test = nullptr; //stand-alone demo/checkout of periphreals
    lv_obj_t * _level_canvas;
    bool _is_in_level = false;
    //Level _active_level=nullptr;

    // Frame Buffers allocations matching your configuration snippets
    uint8_t _canvas_buffer[SCREEN_WIDTH_PX * SCREEN_HEIGHT_PX] __attribute__((aligned(4)));
    uint8_t _level_buffer[SCREEN_WIDTH_PX * SCREEN_HEIGHT_PX] __attribute__((aligned(4))); //LV_COLOR_FORMAT_AL88: byte 0 is luminance 0, and byte 1 is 8-bit opacity
    lv_draw_buf_t _custom_canvas_draw_handle;

    AnimationFunc  _active_animation_lower  = nullptr; // Keeps track of the chosen function address
    AnimationFunc  _active_animation_upper  = nullptr; 

    // Internal Utility Implementations
    static void display_flush_cb(lv_display_t * disp, const lv_area_t * area, uint8_t * px_map);
    static void button_read_cb(lv_indev_t * indev, lv_indev_data_t * data);
    static void menu_event_cb(lv_event_t * e);
    static void menu_focus_cb(lv_event_t * e);
    static void switch_menu(lv_obj_t * new_menu, bool remember_last_selection);
    void led_cb(bool is_menu_event);
    void lvgl2spi(uint8_t* src,Screen &screen);
  public:
    Graphics();
    void begin(SensorSuite &sensor_suite);
    void update();
    void end();

};