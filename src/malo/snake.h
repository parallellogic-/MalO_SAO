#pragma once

#include "game.h"
#include <vector>

// Screen configuration definitions
#define SNAKE_SCREEN_WIDTH 128
#define SNAKE_SCREEN_HEIGHT 121 // 128 - HEADER_HEIGHT_PX

// Grid configuration (Must fit cleanly into screen dimensions)
#define SNAKE_GRID_SIZE 4
#define SNAKE_GRID_COLS (SNAKE_SCREEN_WIDTH / SNAKE_GRID_SIZE) // 32
#define SNAKE_GRID_ROWS (SNAKE_SCREEN_HEIGHT / SNAKE_GRID_SIZE) // 30

enum class SnakeState : uint8_t {
  GAMEPLAY,
  GAME_OVER
};

enum class SnakeDir : uint8_t {
  UP,
  DOWN,
  LEFT,
  RIGHT
};

struct SnakePoint {
  int8_t x;
  int8_t y;
};

class SnakeGame : public Game {
  private:
    SnakeState _game_state = SnakeState::GAMEPLAY;
    uint32_t _frame_id = 0;
    uint8_t _update_speed_frames = 6; // Move snake every X frames

    // Snake attributes
    std::vector<SnakePoint> _snake;
    SnakeDir _current_dir = SnakeDir::RIGHT;
    SnakeDir _next_dir = SnakeDir::RIGHT; // Prevents 180-degree self-collision in a single frame

    // Target object attributes
    SnakePoint _food;
    uint16_t _score = 0;

    // Cooldown/Delay timers
    uint16_t _state_delay_timer = 0;

    // Direct callback tracking flags for narrative triggers
    bool _is_first_food_seen = false;
    bool _is_growth_milestone_seen = false;
    bool _is_close_to_tail_seen = false;

    // Internal engine handlers
    static void _game_draw_cb(lv_event_t* e);
    static void _game_key_cb(lv_event_t* e);
    
    void _spawn_food();
    void _process_movement();
    void _reset_entire_match();

  protected:

  public:
    SnakeGame(const std::string& text, lv_group_t* shared_input_group);
    void begin(bool is_enter_from_above, SensorSuite *sensor_suite) override;
    ScreenAction update() override; 
    void end(bool is_leaving_upward) override;

    lv_key_t touch_to_key(uint8_t touch) override {
        static const lv_key_t touch2key[] = {
            (lv_key_t)0, (lv_key_t)0, LV_KEY_HOME, LV_KEY_ESC, LV_KEY_ENTER,
            LV_KEY_LEFT, LV_KEY_UP, LV_KEY_RIGHT, LV_KEY_LEFT, LV_KEY_DOWN, LV_KEY_RIGHT
        };
        if (touch >= sizeof(touch2key) / sizeof(touch2key[0])) return (lv_key_t)0; 
        return touch2key[touch];
    }
};
