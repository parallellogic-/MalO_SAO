#pragma once

#include "game.h"

// Screen configuration definitions
#define PONG_SCREEN_WIDTH 128
#define PONG_SCREEN_HEIGHT 128 - HEADER_HEIGHT_PX

// Game object dimensions
#define PONG_PADDLE_WIDTH 3
#define PONG_PADDLE_HEIGHT 20
#define PONG_BALL_SIZE 3

enum class PongState : uint8_t {
  GAMEPLAY,
  POINT_SCORED,
  MATCH_OVER
};

class Pong : public Game {
  private:
    PongState _game_state = PongState::GAMEPLAY;
    uint32_t _frame_id = 0;

    // Paddle positions (Y-coordinates represent the top-left corner)
    int16_t _left_paddle_y = (PONG_SCREEN_HEIGHT - PONG_PADDLE_HEIGHT) / 2;  // AI Player
    int16_t _right_paddle_y = (PONG_SCREEN_HEIGHT - PONG_PADDLE_HEIGHT) / 2; // Human Player

    // Ball physics variables
    int16_t _ball_x = PONG_SCREEN_WIDTH / 2;
    int16_t _ball_y = PONG_SCREEN_HEIGHT / 2;
    int8_t _ball_vx = 2; // Horizontal velocity
    int8_t _ball_vy = 1; // Vertical velocity

    // Score tallies
    uint8_t _left_score = 0;  // AI score
    uint8_t _right_score = 0; // Human score

    // Cooldown/Delay timers
    uint16_t _state_delay_timer = 0;

    // Direct callback tracking flags for contextual triggers
    bool _is_first_point_seen = false;
    bool _is_malo_leading_seen = false;
    bool _is_player_rally_seen = false;

    // Internal engine handlers
    static void _game_draw_cb(lv_event_t* e);
    static void _game_key_cb(lv_event_t* e);
    
    void _reset_ball(bool serve_to_right);
    void _update_ai();
    void _process_physics();
    void _reset_entire_match();

  protected:

  public:
    Pong(const std::string& text, lv_group_t* shared_input_group);
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
