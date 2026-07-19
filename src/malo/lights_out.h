#ifndef LIGHTS_OUT_H
#define LIGHTS_OUT_H

#include <Arduino.h>
#include <lvgl.h>
#include "game.h" // Assuming Game base class exists in your framework

#define LO_SCREEN_WIDTH SCREEN_WIDTH_PX
#define LO_SCREEN_HEIGHT (SCREEN_HEIGHT_PX - HEADER_HEIGHT_PX)
#define LO_GRID_SIZE 3
#define LO_CELL_DIM 40 // 60x60px cells

class LightsOut : public Game {
private:
    bool _grid[LO_GRID_SIZE][LO_GRID_SIZE]; // true = ON, false = OFF
    int8_t _cursor_r = 0;
    int8_t _cursor_c = 0;
    bool _is_won = false;

    void _scramble_grid();
    void _toggle_cell(int8_t r, int8_t c);
    void _move_cursor(int8_t dr, int8_t dc);
    void _handle_action();
    bool _check_victory();

    // LVGL drawing callback matching your framework's architecture
    static void _game_draw_cb(lv_event_t* e);
    // Key processing input routing callback 
    static void _game_key_cb(lv_event_t* e);

public:
    LightsOut(const char* name, lv_group_t* input_group) : Game(name, input_group) {}

    void begin(bool is_enter_from_above, SensorSuite *sensor_suite) override;
    void end(bool is_leaving_upward) override;
};

#endif // LIGHTS_OUT_H
