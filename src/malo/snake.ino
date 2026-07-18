#include "snake.h"
#include <cstdlib>

SnakeGame::SnakeGame(const std::string& text, lv_group_t* shared_input_group) : Game(text, shared_input_group) {}

void SnakeGame::begin(bool is_enter_from_above, SensorSuite *sensor_suite) {
  Game::begin(is_enter_from_above, sensor_suite);
  
  if (is_enter_from_above) {
    _overlay_card = nullptr;
    _overlay_timer = nullptr;

    // Attach hardware-accelerated parent engine layout box
    _game_container = lv_obj_create(lv_screen_active()); 
    
    lv_obj_set_size(_game_container, SNAKE_SCREEN_WIDTH, SNAKE_SCREEN_HEIGHT);
    lv_obj_align(_game_container, LV_ALIGN_TOP_LEFT, 0, 128 - SNAKE_SCREEN_HEIGHT);
    
    lv_obj_set_style_bg_color(_game_container, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(_game_container, LV_OPA_COVER, 0); 
    
    lv_obj_set_style_border_width(_game_container, 0, 0);
    lv_obj_set_style_pad_all(_game_container, 0, 0);
    lv_obj_remove_flag(_game_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(_game_container, LV_OBJ_FLAG_HIDDEN);

    lv_obj_add_event_cb(_game_container, _game_draw_cb, LV_EVENT_DRAW_MAIN, this);
    lv_obj_add_event_cb(_game_container, SnakeGame::_game_key_cb, LV_EVENT_KEY, this);

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

void SnakeGame::_reset_entire_match() {
  _score = 0;
  _game_state = SnakeState::GAMEPLAY;
  _current_dir = SnakeDir::RIGHT;
  _next_dir = SnakeDir::RIGHT;
  
  // Clear and initialize snake in center of screen with 3 segments
  _snake.clear();
  int8_t start_x = SNAKE_GRID_COLS / 2;
  int8_t start_y = SNAKE_GRID_ROWS / 2;
  _snake.push_back({start_x, start_y});
  _snake.push_back({(int8_t)(start_x - 1), start_y});
  _snake.push_back({(int8_t)(start_x - 2), start_y});
  
  _spawn_food();
}

void SnakeGame::_spawn_food() {
  bool valid_position = false;
  while (!valid_position) {
    _food.x = std::rand() % SNAKE_GRID_COLS;
    _food.y = std::rand() % SNAKE_GRID_ROWS;
    
    valid_position = true;
    // Check if food coordinates spawn inside the body of the snake
    for (const auto& segment : _snake) {
      if (segment.x == _food.x && segment.y == _food.y) {
        valid_position = false;
        break;
      }
    }
  }
}

void SnakeGame::_process_movement() {
  _current_dir = _next_dir;

  // Calculate new target head position coordinate bounds
  SnakePoint head = _snake.front();
  switch (_current_dir) {
    case SnakeDir::UP:    head.y--; break;
    case SnakeDir::DOWN:  head.y++; break;
    case SnakeDir::LEFT:  head.x--; break;
    case SnakeDir::RIGHT: head.x++; break;
  }

  // Bounding box screen collision parameters (Walls are lethal)
  if (head.x < 0 || head.x >= SNAKE_GRID_COLS || head.y < 0 || head.y >= SNAKE_GRID_ROWS) {
    _game_state = SnakeState::GAME_OVER;
    _state_delay_timer = 90;
    return;
  }

  // Self-collision verification checks
  for (size_t i = 0; i < _snake.size(); i++) {
    if (head.x == _snake[i].x && head.y == _snake[i].y) {
      _game_state = SnakeState::GAME_OVER;
      _state_delay_timer = 90;
      return;
    }
  }

  // Move snake head forward
  _snake.insert(_snake.begin(), head);

  // Check if head matches active target coordinates
  if (head.x == _food.x && head.y == _food.y) {
    _score++;
    _spawn_food();

    // Narrative trigger systems based on score markers
    if (!_is_first_food_seen) {
      _create_popup_overlay("Application MalO ver1.0.0:\nI love watching you grow.");
      _is_first_food_seen = true;
    } else if (_score >= 10 && !_is_growth_milestone_seen) {
      _create_popup_overlay("YOU CANNOT OUTRUN\nYOUR OWN ARCHIVE.");
      _is_growth_milestone_seen = true;
    } else if (_snake.size() > 15 && !_is_close_to_tail_seen) {
      _create_popup_overlay("MalO is monitoring your micro-adjustments.");
      _is_close_to_tail_seen = true;
    } else {
      _create_popup_overlay("IMAGE REFRESH SUCCESSFUL.");
    }
  } else {
    // Trim tail if no target item was consumed this physics step
    _snake.pop_back();
  }
}

ScreenAction SnakeGame::update() {
  _frame_id++;
  _update_action.led_upper_func = &Charlieplex::animation_off;
  _update_action.led_lower_func = &Charlieplex::animation_off;

  if (_game_state == SnakeState::GAMEPLAY) {
    // Run physics frame intervals based on targeted engine refresh updates
    if (_frame_id % _update_speed_frames == 0) {
      _process_movement();
    }
  } else if (_game_state == SnakeState::GAME_OVER) {
    if (_state_delay_timer > 0) {
      _state_delay_timer--;
    } else {
      // Unlock state parameters if score requirement is reached
      if (_score >= 15 && _sensor_suite) {
        _sensor_suite->save_state.unlock("Pong Champ");
      }
      _reset_entire_match();
    }
  }

  lv_obj_invalidate(_game_container);
  return Game::update();
}

void SnakeGame::_game_key_cb(lv_event_t* e) {
  SnakeGame* instance = (SnakeGame*)lv_event_get_user_data(e);
  uint32_t key = lv_event_get_key(e);

  if (instance->_game_state != SnakeState::GAMEPLAY) return;

  // Process instant direction modifications securely via clean callback events
  switch (key) {
    case LV_KEY_UP:
      if (instance->_current_dir != SnakeDir::DOWN) {
        instance->_next_dir = SnakeDir::UP;
      }
      break;
    case LV_KEY_DOWN:
      if (instance->_current_dir != SnakeDir::UP) {
        instance->_next_dir = SnakeDir::DOWN;
      }
      break;
    case LV_KEY_LEFT:
      if (instance->_current_dir != SnakeDir::RIGHT) {
        instance->_next_dir = SnakeDir::LEFT;
      }
      break;
    case LV_KEY_RIGHT:
      if (instance->_current_dir != SnakeDir::LEFT) {
        instance->_next_dir = SnakeDir::RIGHT;
      }
      break;
    case LV_KEY_HOME:
      instance->_update_action.type = ScreenActionType::PUSH_SUBMENU;
      instance->_update_action.next_screen = instance->_screen_stack.empty() ? nullptr : instance->_screen_stack.front();
      break;
  }
}

void SnakeGame::_game_draw_cb(lv_event_t* e) {
  lv_layer_t* layer = lv_event_get_layer(e);
  SnakeGame* instance = (SnakeGame*)lv_event_get_user_data(e);

  // Initialize base layout rectangular graphic styling structures
  lv_draw_rect_dsc_t rect_dsc;
  lv_draw_rect_dsc_init(&rect_dsc);
  rect_dsc.bg_opa = LV_OPA_COVER;

  // 1. Render Target Food Element
  rect_dsc.bg_color = lv_color_white();
  lv_area_t food_area;
  food_area.x1 = instance->_food.x * SNAKE_GRID_SIZE;
  food_area.y1 = instance->_food.y * SNAKE_GRID_SIZE;
  food_area.x2 = food_area.x1 + SNAKE_GRID_SIZE - 1;
  food_area.y2 = food_area.y1 + SNAKE_GRID_SIZE - 1;
  lv_draw_rect(layer, &rect_dsc, &food_area);

  // 2. Render Active Snake Segments
  for (size_t i = 0; i < instance->_snake.size(); i++) {
    // Softly dim body fragments if flashing on game over conditions
    if (instance->_game_state == SnakeState::GAME_OVER && (instance->_frame_id % 10 < 5)) {
      continue;
    }
    
    // Give head a distinctive full coloring, body segments slightly offset if desired
    rect_dsc.bg_color = lv_color_white();
    
    lv_area_t segment_area;
    segment_area.x1 = instance->_snake[i].x * SNAKE_GRID_SIZE;
    segment_area.y1 = instance->_snake[i].y * SNAKE_GRID_SIZE;
    segment_area.x2 = segment_area.x1 + SNAKE_GRID_SIZE - 1;
    segment_area.y2 = segment_area.y1 + SNAKE_GRID_SIZE - 1;
    lv_draw_rect(layer, &rect_dsc, &segment_area);
  }
}

void SnakeGame::end(bool is_leaving_upward) {
  if (is_leaving_upward) {
    if (_game_container) {
      lv_obj_del(_game_container);
      _game_container = nullptr;
    }
  } else {
    lv_obj_add_flag(_game_container, LV_OBJ_FLAG_HIDDEN);
  }
}
