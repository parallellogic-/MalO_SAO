#pragma once

#include "game.h"
#include <vector>

#define PIPE_SCREEN_WIDTH SCREEN_WIDTH_PX
#define PIPE_SCREEN_HEIGHT (SCREEN_HEIGHT_PX - HEADER_HEIGHT_PX)

#define PIPE_GRID_COLS 8
#define PIPE_GRID_ROWS 6
#define PIPE_CELL_SIZE (PIPE_SCREEN_WIDTH / PIPE_GRID_COLS)

enum class PipeState : uint8_t {
    GAMEPLAY,
    FAST_FORWARD,
    GAME_OVER,
    WIN
};

enum class PipeType : uint8_t {
    STRAIGHT,   // Connects parallel ends
    BEND,       // 90-degree curve
    THREE_WAY,  // T-junction
    FOUR_WAY    // Full cross
};

struct PipePiece {
    PipeType type;
    uint8_t rotation;    // 0: 0°, 1: 90°, 2: 180°, 3: 270° CW
    bool filled;         // Liquid filled flag
    float fluid_progress;// 0.0f to 1.0f progress within cell
};

class PipeGame : public Game {
  private:
    PipeState _game_state = PipeState::GAMEPLAY;
    PipePiece _grid[PIPE_GRID_ROWS][PIPE_GRID_COLS];
    
    // Cursor location (center cell)
    int8_t _cursor_x = 3;
    int8_t _cursor_y = 3;

    // Fluid flow mechanics
    int8_t _fluid_x = 0;
    int8_t _fluid_y = 0;
    int8_t _fluid_enter_dir = 3; // 0: Top, 1: Right, 2: Bottom, 3: Left
    
    uint16_t _start_delay_ticks = 150; // Initial delay frames before fluid flows
    float _fluid_speed = 0.02f;
    bool _is_path_viable = false;

    // Internal pipeline helpers
    static void _game_draw_cb(lv_event_t* e);
    static void _game_key_cb(lv_event_t* e);
    
    void _generate_solvable_board();
    void _rotate_cursor(PipeGame* self, bool clockwise);
    void _rotate_piece(PipeGame* self, int8_t y, int8_t x, bool clockwise);
    void _process_fluid_simulation();
    bool _get_connections(int8_t y, int8_t x, bool connections[4]);
    bool _check_full_solvability(int8_t start_y, int8_t start_x, int8_t enter_dir);

  public:
    PipeGame(const std::string& text, lv_group_t* shared_input_group);
    void begin(bool is_enter_from_above, SensorSuite *sensor_suite) override;
    ScreenAction update() override; 
    void end(bool is_leaving_upward) override;

    lv_key_t touch_to_key(uint8_t touch) override{
        static const lv_key_t touch2key[] = {(lv_key_t)0,(lv_key_t)0,LV_KEY_HOME,LV_KEY_ESC,LV_KEY_ENTER,(lv_key_t)';',LV_KEY_UP,(lv_key_t)'/',LV_KEY_LEFT,LV_KEY_DOWN,LV_KEY_RIGHT};
        //unused, hidden, menu, no, yes, CCW, up, CW, left, down, right
        if(touch>=sizeof(touch2key)/sizeof(touch2key[0])) return (lv_key_t)0; return touch2key[touch]; }
};
