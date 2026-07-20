#pragma once

#include "game.h"
#include <vector>

// Screen configuration definitions
#define LABY_SCREEN_WIDTH 128
#define LABY_SCREEN_HEIGHT (128 - HEADER_HEIGHT_PX)

// Physics Configurations 
#define LABY_GRID_SIZE 8
#define LABY_GRID_COLS (LABY_SCREEN_WIDTH / LABY_GRID_SIZE)  // 16
#define LABY_GRID_ROWS (LABY_SCREEN_HEIGHT / LABY_GRID_SIZE) // 15
#define LABY_BALL_RADIUS 2

enum class LabyState : uint8_t {
  GAMEPLAY,
  HOLE_FALL,
  VICTORY
};

enum class CellType : uint8_t {
  EMPTY = 0,
  WALL  = 1,
  HOLE  = 2,
  START = 3,
  GOAL  = 4
};

struct LabyPoint {
  int16_t x;
  int16_t y;
};

class LabyrinthGame : public Game {
  private:
    uint8_t _current_level = 0;
    static const uint8_t LABY_MAX_LEVELS = 3; // Change this to your level count
    static const uint8_t _maps[LABY_MAX_LEVELS][LABY_GRID_ROWS][LABY_GRID_COLS];
    
    LabyState _game_state = LabyState::GAMEPLAY;
    uint32_t _frame_id = 0;
    uint16_t _state_delay_timer = 0;

    // Sub-pixel high-precision physics accumulation variables
    float _ball_x = 0.0f;
    float _ball_y = 0.0f;
    float _ball_vx = 0.0f;
    float _ball_vy = 0.0f;

    // Static 16x15 structural layout memory array map
    static const uint8_t _map[LABY_GRID_ROWS][LABY_GRID_COLS];

    LabyPoint _start_pos;
    LabyPoint _goal_pos;
    std::vector<LabyPoint> _holes;

    // Internal engine handlers
    static void _game_draw_cb(lv_event_t* e);
    static void _game_key_cb(lv_event_t* e);
    
    void _parse_map();
    void _process_physics();
    void _reset_ball();

  protected:

  public:
    LabyrinthGame(const std::string& text, lv_group_t* shared_input_group);
    void begin(bool is_enter_from_above, SensorSuite *sensor_suite) override;
    ScreenAction update() override; 
    void end(bool is_leaving_upward) override;
};
