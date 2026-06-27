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
    
    // Core Layout Pointers
    lv_obj_t* _main_canvas = nullptr;
    lv_obj_t* _title_bar = nullptr;
    lv_obj_t* _content_area = nullptr;
    lv_obj_t* _menu_list = nullptr;
    lv_obj_t* _overlay_panel = nullptr;

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
    uint8_t _active_level_id = 0; // 0=None, 1=TicTacToe, 2=Puzzle...

    // Frame Buffers allocations matching your configuration snippets
    uint8_t _canvas_buffer[SCREEN_WIDTH_PX * SCREEN_HEIGHT_PX] __attribute__((aligned(4)));
    lv_draw_buf_t _custom_canvas_draw_handle;

    // Internal Utility Implementations
    void build_base_ui_frame();
    void draw_title_bar(SensorSuite& sensors);
    void build_tos_screen();
    void build_menu_tree(const char* options[], uint8_t count);
    void handle_menu_navigation(uint8_t key);
    void handle_name_anim_input(uint8_t key, SensorSuite& sensors);
    void process_achievement_queue();
    void initialize_level_subsystem(uint8_t level_id);
    void lvgl2spi(Screen &screen);
  public:
    Graphics();
    void begin();
    void update(SensorSuite &sensor_suite);
    void end();

    void trigger_achievement_overlay(uint8_t achievement_id);
    void save_state_to_disk(SensorSuite& sensors);
    void provision_default_save(SensorSuite& sensors);
};