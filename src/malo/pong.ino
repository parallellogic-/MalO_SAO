#include "pong.h"
#include <cstdlib>

Pong::Pong(const std::string& text, lv_group_t* shared_input_group) : Game(text, shared_input_group) {}

void Pong::begin(bool is_enter_from_above, SensorSuite *sensor_suite) {
  Game::begin(is_enter_from_above, sensor_suite);
  
  if (is_enter_from_above) {
    _overlay_card = nullptr;
    _overlay_timer = nullptr;

    // Attach hardware-accelerated parent engine layout box
    _game_container = lv_obj_create(lv_screen_active()); 
    
    lv_obj_set_size(_game_container, PONG_SCREEN_WIDTH, PONG_SCREEN_HEIGHT);
    lv_obj_align(_game_container, LV_ALIGN_TOP_LEFT, 0, HEADER_HEIGHT_PX);
    
    lv_obj_set_style_bg_color(_game_container, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(_game_container, LV_OPA_COVER, 0); 
    
    lv_obj_set_style_border_width(_game_container, 0, 0);
    lv_obj_set_style_pad_all(_game_container, 0, 0);
    lv_obj_remove_flag(_game_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(_game_container, LV_OBJ_FLAG_HIDDEN);

    lv_obj_add_event_cb(_game_container, _game_draw_cb, LV_EVENT_DRAW_MAIN, this);
    lv_obj_add_event_cb(_game_container, Pong::_game_key_cb, LV_EVENT_KEY, this);

    _reset_entire_match();
  } else {
    lv_obj_clear_flag(_game_container, LV_OBJ_FLAG_HIDDEN);
  }

  if (_input_group) { 
      lv_group_add_obj(_input_group, _game_container);
      lv_group_focus_obj(_game_container);
  }

  lv_obj_invalidate(_game_container);
}

void Pong::_reset_entire_match() {
  _left_score = 0;
  _right_score = 0;
  _game_state = PongState::GAMEPLAY;
  _left_paddle_y = (PONG_SCREEN_HEIGHT - PONG_PADDLE_HEIGHT) / 2;
  _right_paddle_y = (PONG_SCREEN_HEIGHT - PONG_PADDLE_HEIGHT) / 2;
  _reset_ball(true);
}

void Pong::_reset_ball(bool serve_to_right) {
  _ball_x = PONG_SCREEN_WIDTH / 2;
  _ball_y = PONG_SCREEN_HEIGHT / 2;
  
  // Randomize the horizontal serve direction (ignores who scored last for complete randomness)
  bool random_serve_right = (std::rand() % 2 == 0);
  _ball_vx = random_serve_right ? 2 : -2;
  
  // Guarantee initial vertical velocity is non-zero (either 1 or -1)
  _ball_vy = (std::rand() % 2 == 0) ? 1 : -1;
}

void Pong::_update_ai() {
  // Target center of the ball position coordinates
  int16_t ball_center_y = _ball_y + (PONG_BALL_SIZE / 2);
  int16_t paddle_center_y = _left_paddle_y + (PONG_PADDLE_HEIGHT / 2);

  // Implement a tracking filter loop with speed thresholds to keep AI beatable
  if (ball_center_y < paddle_center_y - 2) {
    _left_paddle_y -= 1;
  } else if (ball_center_y > paddle_center_y + 2) {
    _left_paddle_y += 1;
  }

  // Contain bounds verification routines
  if (_left_paddle_y < 0) _left_paddle_y = 0;
  if (_left_paddle_y > PONG_SCREEN_HEIGHT - PONG_PADDLE_HEIGHT) {
    _left_paddle_y = PONG_SCREEN_HEIGHT - PONG_PADDLE_HEIGHT;
  }
}

void Pong::_process_physics() {
  _ball_x += _ball_vx;
  _ball_y += _ball_vy;

  // Ceiling and Floor bounding box collision bounces
  if (_ball_y <= 0) {
    _ball_y = 0;
    _ball_vy = -_ball_vy;
  } else if (_ball_y >= PONG_SCREEN_HEIGHT - PONG_BALL_SIZE) {
    _ball_y = PONG_SCREEN_HEIGHT - PONG_BALL_SIZE;
    _ball_vy = -_ball_vy;
  }

  // Left Paddle Collision (AI)
  if (_ball_x <= PONG_PADDLE_WIDTH) {
    if (_ball_y + PONG_BALL_SIZE >= _left_paddle_y && _ball_y <= _left_paddle_y + PONG_PADDLE_HEIGHT) {
      _ball_x = PONG_PADDLE_WIDTH;
      
      _ball_vx = -_ball_vx;
      if (_ball_vx < 2) _ball_vx = 2; 

      // Introduce velocity variability based on collision point offsets
      int16_t hit_pos = (_ball_y + PONG_BALL_SIZE / 2) - (_left_paddle_y + PONG_PADDLE_HEIGHT / 2);
      _ball_vy = hit_pos / 4;
      
      // Prevent flat horizontal traps: force a minimum vertical bounce direction
      if (_ball_vy == 0) {
        _ball_vy = (hit_pos >= 0) ? 1 : -1;
      }
    } else if (_ball_x < 0) {
      // Right (Human Player) Scores!
      _right_score++;
      _game_state = PongState::POINT_SCORED;
      _state_delay_timer = 45; 
      
      if (!_is_player_rally_seen && _right_score >= 3) {
        //_create_popup_overlay("Hesitation noted,\nupdating response time.");
        _is_player_rally_seen = true;
      } else {
        //_create_popup_overlay("POINT FOR USER.\nUPDATING GEOMETRY.");
      }
    }
  }

  // Right Paddle Collision (Human)
  if (_ball_x >= PONG_SCREEN_WIDTH - PONG_PADDLE_WIDTH - PONG_BALL_SIZE) {
    if (_ball_y + PONG_BALL_SIZE >= _right_paddle_y && _ball_y <= _right_paddle_y + PONG_PADDLE_HEIGHT) {
      _ball_x = PONG_SCREEN_WIDTH - PONG_PADDLE_WIDTH - PONG_BALL_SIZE;
      
      _ball_vx = -_ball_vx;
      if (_ball_vx > -2) _ball_vx = -2;

      int16_t hit_pos = (_ball_y + PONG_BALL_SIZE / 2) - (_right_paddle_y + PONG_PADDLE_HEIGHT / 2);
      _ball_vy = hit_pos / 4;
      
      // Prevent flat horizontal traps: force a minimum vertical bounce direction
      if (_ball_vy == 0) {
        _ball_vy = (hit_pos >= 0) ? 1 : -1;
      }
    } else if (_ball_x > PONG_SCREEN_WIDTH) {
      // Left (AI Player) Scores!
      _left_score++;
      _game_state = PongState::POINT_SCORED;
      _state_delay_timer = 45;

      if (!_is_first_point_seen) {
        //_create_popup_overlay("I am right behind you.");
        _is_first_point_seen = true;
      } else if (_left_score > _right_score && !_is_malo_leading_seen) {
        //_create_popup_overlay("YOU CANNOT OUTRUN\nYOUR DESTINY.");
        _is_malo_leading_seen = true;
      } else {
        //_create_popup_overlay("IMAGE REFRESH\nSUCESSFUL.");
      }
    }
  }

  // End Condition Assertions (Play to 5 Points)
  if (_left_score >= 5 || _right_score >= 5) {
    _game_state = PongState::MATCH_OVER;
    _state_delay_timer = 90;
  }
}

ScreenAction Pong::update() {
  _update_action.led_upper_func = &Charlieplex::animation_off;
  _update_action.led_lower_func = &Charlieplex::animation_off;

  if (_game_state == PongState::GAMEPLAY) {
    bool key_pressed_this_frame = false;

    // Poll direct hardware key status to remove button processing latency
    lv_indev_t* indev = lv_indev_get_next(nullptr);
    while (indev) {
      if (lv_indev_get_type(indev) == LV_INDEV_TYPE_KEYPAD) {
        // Check if the input device is actively pressed
        if (lv_indev_get_state(indev) == LV_INDEV_STATE_PRESSED) {
          uint32_t active_key = lv_indev_get_key(indev);
          
          if (active_key == LV_KEY_UP) {
            _right_paddle_y -= 3; 
            if (_right_paddle_y < 0) _right_paddle_y = 0;
            key_pressed_this_frame = true;
          } else if (active_key == LV_KEY_DOWN) {
            _right_paddle_y += 3;
            if (_right_paddle_y > PONG_SCREEN_HEIGHT - PONG_PADDLE_HEIGHT) {
              _right_paddle_y = PONG_SCREEN_HEIGHT - PONG_PADDLE_HEIGHT;
            }
            key_pressed_this_frame = true;
          }
        }
      }
      indev = lv_indev_get_next(indev);
    }

    // Explicitly halt paddle movement if no vertical inputs are active
    if (!key_pressed_this_frame) {
      // Paddle position remains unchanged (_right_paddle_y = _right_paddle_y)
    }

    _update_ai();
    _process_physics();
  } else if (_game_state == PongState::POINT_SCORED) {
    if (_state_delay_timer > 0) {
      _state_delay_timer--;
    } else {
      _game_state = PongState::GAMEPLAY;
      _reset_ball(_left_score > _right_score);
    }
  } else if (_game_state == PongState::MATCH_OVER) {
    if (_state_delay_timer > 0) {
      _state_delay_timer--;
    } else {
      if (_right_score >= 5 && _sensor_suite) {
        _sensor_suite->save_state.unlock("Pong Champ");
      }
      _reset_entire_match();
    }
  }

  lv_obj_invalidate(_game_container);
  return Game::update();
}


void Pong::_game_key_cb(lv_event_t* e) {
  Pong* instance = (Pong*)lv_event_get_user_data(e);
  uint32_t key = lv_event_get_key(e);

  // Keep menu routing / back buttons tied strictly to clean callback events
  if (key == LV_KEY_HOME) {
    instance->_update_action.type = ScreenActionType::PUSH_SUBMENU;
    instance->_update_action.next_screen = instance->_screen_stack.empty() ? nullptr : instance->_screen_stack.front();
  }
}

void Pong::_game_draw_cb(lv_event_t* e) {
  lv_layer_t* layer = lv_event_get_layer(e);
  Pong* instance = (Pong*)lv_event_get_user_data(e);
  lv_obj_t* obj = (lv_obj_t*)lv_event_get_target(e);
  lv_area_t container_coords;
  lv_obj_get_coords(obj, &container_coords);

  // Initialize base geometric design brush properties
  lv_draw_rect_dsc_t rect_dsc;
  lv_draw_rect_dsc_init(&rect_dsc);
  rect_dsc.bg_color = lv_color_white();
  rect_dsc.bg_opa = LV_OPA_COVER;

  // 1. Draw Mid-field Dotted Vector Net Divider
  for (int16_t y = 2; y < PONG_SCREEN_HEIGHT; y += 8) {
    lv_area_t net_area;
    net_area.x1 = container_coords.x1+(PONG_SCREEN_WIDTH / 2) - 1;
    net_area.y1 = container_coords.y1+y;
    net_area.x2 = container_coords.x1+(PONG_SCREEN_WIDTH / 2);
    net_area.y2 = container_coords.y1+y + 4;
    lv_draw_rect(layer, &rect_dsc, &net_area);
  }

  // 2. Render Left Paddle (AI)
  lv_area_t left_paddle;
  left_paddle.x1 = container_coords.x1+0;
  left_paddle.y1 = container_coords.y1+instance->_left_paddle_y;
  left_paddle.x2 = container_coords.x1+PONG_PADDLE_WIDTH;
  left_paddle.y2 = container_coords.y1+instance->_left_paddle_y + PONG_PADDLE_HEIGHT;
  lv_draw_rect(layer, &rect_dsc, &left_paddle);

  // 3. Render Right Paddle (Human Player)
  lv_area_t right_paddle;
  right_paddle.x1 = container_coords.x1+PONG_SCREEN_WIDTH - PONG_PADDLE_WIDTH;
  right_paddle.y1 = container_coords.y1+instance->_right_paddle_y;
  right_paddle.x2 = container_coords.x1+PONG_SCREEN_WIDTH;
  right_paddle.y2 = container_coords.y1+instance->_right_paddle_y + PONG_PADDLE_HEIGHT;
  lv_draw_rect(layer, &rect_dsc, &right_paddle);

  // 4. Render Active Target Ball Matrix
  if (instance->_game_state == PongState::GAMEPLAY || (instance->_frame_id % 10 < 7)) {
    lv_area_t ball;
    ball.x1 = container_coords.x1+instance->_ball_x;
    ball.y1 = container_coords.y1+instance->_ball_y;
    ball.x2 = container_coords.x1+instance->_ball_x + PONG_BALL_SIZE;
    ball.y2 = container_coords.y1+instance->_ball_y + PONG_BALL_SIZE;
    lv_draw_rect(layer, &rect_dsc, &ball);
  }
}

void Pong::end(bool is_leaving_upward) {
  if (is_leaving_upward) {
    if (_game_container) {
      lv_obj_del(_game_container);
      _game_container = nullptr;
    }
  } else {
    lv_obj_add_flag(_game_container, LV_OBJ_FLAG_HIDDEN);
  }
}
