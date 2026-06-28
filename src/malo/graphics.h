#pragma once

#include "malo.h"
#include "screen.h"

//WAS ~/Arduino/libraries/lv_conf.h
//IS ~/Arduino/libraries/lvgl/src/lv_conf.h
#define LV_CONF_INCLUDE_SIMPLE
#include <lvgl.h>

// Forward declaration tells the compiler this struct exists
struct SensorSuite; 

class Graphics{
  private:
//    MenuState _current_state = MenuState::MAIN_MENU;
//    MenuState _previous_state = MenuState::MAIN_MENU;
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


    // Core Layout Pointers
    /*lv_obj_t* _main_canvas = nullptr;
    lv_obj_t* _title_bar = nullptr;
    lv_obj_t* _content_area = nullptr;
    lv_obj_t* _menu_list = nullptr;
    lv_obj_t* _overlay_panel = nullptr;

    lv_display_t* _dummy_disp = nullptr;//debug

    // Title Bar Metrics labels
    lv_obj_t* _lbl_battery = nullptr;
    lv_obj_t* _lbl_unlocks = nullptr;
    lv_obj_t* _lbl_ir = nullptr;

    // Temporary Variable Data States
    int8_t _selected_menu_idx = 0;
    int8_t _selected_submenu_idx = 0;
    int8_t _active_name_char_idx = 0;
    uint32_t _button_hold_timer = 0;
    
    // Achievement Overlay Handling
    bool _overlay_active = false;
    uint32_t _overlay_timestamp = 0;
    uint8_t _pending_achievement_id = 0;

    // Level Integration Instances
//    ActiveLevelContainer _level_pool;
    uint8_t _active_level_id = 0;*/ // 0=None, 1=TicTacToe, 2=Puzzle...

    // Frame Buffers allocations matching your configuration snippets
    uint8_t _canvas_buffer[SCREEN_WIDTH_PX * SCREEN_HEIGHT_PX] __attribute__((aligned(4)));
    lv_draw_buf_t _custom_canvas_draw_handle;

    // Internal Utility Implementations
    static void display_flush_cb(lv_display_t * disp, const lv_area_t * area, uint8_t * px_map);
    static void button_read_cb(lv_indev_t * indev, lv_indev_data_t * data);
    static void menu_event_cb(lv_event_t * e);
    static void menu_focus_cb(lv_event_t * e);
    static void switch_menu(lv_obj_t * new_menu, bool remember_last_selection);
    void lvgl2spi(uint8_t* src,Screen &screen);
  public:
    Graphics();
    void begin(SensorSuite &sensor_suite);
    void update();
    void end();
};