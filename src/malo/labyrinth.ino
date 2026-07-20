#include "labyrinth.h"
#include <cmath>

// Define a structured 16x15 classic static maze layout
// 1 = Wall, 2 = Hole, 3 = Start, 4 = Goal, 0 = Empty Corridor
const uint8_t LabyrinthGame::_maps[LABY_MAX_LEVELS][LABY_GRID_ROWS][LABY_GRID_COLS] = {
  // --- LEVEL 1 ---
  {
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
    {1,3,0,0,1,0,0,2,0,0,0,0,1,0,4,1},
    {1,1,1,0,1,0,1,1,1,1,1,0,1,0,1,1},
    {1,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1},
    {1,0,1,1,1,1,1,1,2,0,1,1,1,1,0,1},
    {1,0,1,0,0,0,0,1,0,0,1,0,0,1,0,1},
    {1,0,1,0,1,1,0,1,0,0,1,0,2,1,0,1},
    {1,0,0,0,1,0,0,0,0,0,0,0,0,1,0,1},
    {1,1,1,0,1,1,1,1,1,2,1,1,0,1,0,1},
    {1,0,0,0,0,0,1,0,0,0,0,1,0,0,0,1},
    {1,0,1,1,1,0,1,0,1,1,0,1,1,1,0,1},
    {1,2,1,0,0,0,1,0,1,0,0,0,0,1,0,1},
    {1,0,1,0,1,1,1,0,1,1,1,1,0,1,0,1},
    {1,0,0,0,0,0,0,0,2,0,0,0,0,0,0,1},
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1}
  },
  // --- LEVEL 3 ---
  {
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
    {1,3,1,0,0,0,2,0,0,2,0,0,0,1,4,1},
    {1,0,1,0,1,1,1,1,1,1,1,1,0,1,0,1},
    {1,0,0,0,1,0,0,0,0,0,0,1,0,0,0,1},
    {1,1,1,0,1,0,1,1,1,1,0,1,0,1,1,1},
    {1,0,0,0,0,0,1,0,2,1,0,1,0,0,0,1},
    {1,0,1,1,1,1,1,0,1,1,1,1,1,1,0,1},
    {1,0,1,0,0,0,0,0,0,0,0,0,0,1,0,1},
    {1,0,1,0,1,1,1,0,1,1,1,0,1,1,0,1},
    {1,0,0,0,1,2,1,0,1,0,1,0,0,0,0,1},
    {1,1,1,0,1,0,1,0,1,0,1,0,1,1,1,1},
    {1,2,1,0,0,0,0,0,0,0,0,2,1,2,0,1},
    {1,0,1,1,1,1,1,0,1,1,1,1,1,1,0,1},
    {1,0,0,0,0,0,2,0,2,0,0,0,0,0,0,1},
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1}
  },
  // --- LEVEL 2 ---
  {
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
    {1,3,0,0,0,0,2,0,0,0,1,1,0,0,4,1},
    {1,0,1,1,1,0,1,1,1,0,1,1,0,1,1,1},
    {1,0,1,2,1,0,0,0,1,0,0,0,0,1,2,1},
    {1,0,1,0,1,1,1,0,1,1,1,1,0,1,0,1},
    {1,0,0,0,0,0,1,0,0,0,0,1,0,0,0,1},
    {1,1,1,2,1,0,1,1,1,1,0,1,0,1,1,1},
    {1,0,0,0,1,0,0,0,2,1,0,0,0,0,0,1},
    {1,0,1,1,1,1,1,0,1,1,1,1,1,1,0,1},
    {1,0,1,0,0,0,0,0,0,0,0,0,0,1,0,1},
    {1,0,1,0,1,1,1,2,1,1,1,1,0,1,0,1},
    {1,0,0,0,1,0,0,0,0,0,0,1,0,0,0,1},
    {1,1,1,0,1,0,1,1,1,1,0,1,0,1,1,1},
    {1,2,0,0,0,0,1,0,0,2,0,0,0,0,2,1},
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1}
  }
};

LabyrinthGame::LabyrinthGame(const std::string& text, lv_group_t* shared_input_group) : Game(text, shared_input_group) {}

void LabyrinthGame::begin(bool is_enter_from_above, SensorSuite *sensor_suite) {
  Game::begin(is_enter_from_above, sensor_suite);
  if (is_enter_from_above) {
    _overlay_card = nullptr;
    _overlay_timer = nullptr;
    _current_level = 0; // 🚀 Start fresh on Level 1 when transitioning from above

    _game_container = lv_obj_create(lv_screen_active()); 
    
    lv_obj_set_size(_game_container, LABY_SCREEN_WIDTH, LABY_SCREEN_HEIGHT);
    lv_obj_align(_game_container, LV_ALIGN_TOP_LEFT, 0, 128 - LABY_SCREEN_HEIGHT);
    
    lv_obj_set_style_bg_color(_game_container, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(_game_container, LV_OPA_COVER, 0); 
    
    lv_obj_set_style_border_width(_game_container, 0, 0);
    lv_obj_set_style_pad_all(_game_container, 0, 0);
    lv_obj_remove_flag(_game_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(_game_container, LV_OBJ_FLAG_HIDDEN);

    lv_obj_add_event_cb(_game_container, _game_draw_cb, LV_EVENT_DRAW_MAIN, this);
    lv_obj_add_event_cb(_game_container, LabyrinthGame::_game_key_cb, LV_EVENT_KEY, this);

    _parse_map();
    _reset_ball();
    _game_state = LabyState::GAMEPLAY;
    _create_popup_overlay("Level 1\nTilt!");
  } else {
    lv_obj_clear_flag(_game_container, LV_OBJ_FLAG_HIDDEN);
  }

  if (_input_group) { 
      lv_group_add_obj(_input_group, _game_container);
      lv_group_focus_obj(_game_container);
  }

  lv_obj_invalidate(_game_container);
}

void LabyrinthGame::_parse_map() {
  _holes.clear();
  // 🚀 FIXED: Read tracking configurations directly from the active level matrix pointer
  for (int8_t r = 0; r < LABY_GRID_ROWS; r++) {
    for (int8_t c = 0; c < LABY_GRID_COLS; c++) {
      uint8_t cell = _maps[_current_level][r][c];
      if (cell == (uint8_t)CellType::START) {
        _start_pos = { (int16_t)(c * LABY_GRID_SIZE + LABY_GRID_SIZE / 2), (int16_t)(r * LABY_GRID_SIZE + LABY_GRID_SIZE / 2) };
      } else if (cell == (uint8_t)CellType::GOAL) {
        _goal_pos = { (int16_t)(c * LABY_GRID_SIZE + LABY_GRID_SIZE / 2), (int16_t)(r * LABY_GRID_SIZE + LABY_GRID_SIZE / 2) };
      } else if (cell == (uint8_t)CellType::HOLE) {
        _holes.push_back({ (int16_t)(c * LABY_GRID_SIZE + LABY_GRID_SIZE / 2), (int16_t)(r * LABY_GRID_SIZE + LABY_GRID_SIZE / 2) });
      }
    }
  }
}

void LabyrinthGame::_reset_ball() {
  _ball_x = (float)_start_pos.x;
  _ball_y = (float)_start_pos.y;
  _ball_vx = 0.0f;
  _ball_vy = 0.0f;
}

void LabyrinthGame::_process_physics() {
  if (!_sensor_suite) return;

  float ax = -_sensor_suite->imu.get_accel(1) * 0.15f; 
  float ay =  _sensor_suite->imu.get_accel(0) * 0.15f;

  _ball_vx = (_ball_vx + ax) * 0.92f;
  _ball_vy = (_ball_vy + ay) * 0.92f;

  float next_x = _ball_x + _ball_vx;
  bool collision_x = false;

  // 🚀 FIXED: Capture active level scope environment dynamically inside lambda check
  auto hits_wall = [this](float check_x, float check_y) -> bool {
    int8_t col = (int8_t)(check_x / LABY_GRID_SIZE);
    int8_t row = (int8_t)(check_y / LABY_GRID_SIZE);
    if (col < 0 || col >= LABY_GRID_COLS || row < 0 || row >= LABY_GRID_ROWS) return true;
    return (_maps[_current_level][row][col] == (uint8_t)CellType::WALL);
  };

  if (hits_wall(next_x - LABY_BALL_RADIUS, _ball_y) || hits_wall(next_x + LABY_BALL_RADIUS, _ball_y)) {
    collision_x = true;
  }

  if (!collision_x) {
    _ball_x = next_x;
  } else {
    _ball_vx = 0.0f; 
  }

  float next_y = _ball_y + _ball_vy;
  bool collision_y = false;

  if (hits_wall(_ball_x, next_y - LABY_BALL_RADIUS) || hits_wall(_ball_x, next_y + LABY_BALL_RADIUS)) {
    collision_y = true;
  }

  if (!collision_y) {
    _ball_y = next_y;
  } else {
    _ball_vy = 0.0f;
  }

  for (const auto& hole : _holes) {
    float dx = _ball_x - hole.x;
    float dy = _ball_y - hole.y;
    float distance = std::sqrt(dx * dx + dy * dy);
    
    if (distance < (LABY_GRID_SIZE / 2 - 1)) {
      _game_state = LabyState::HOLE_FALL;
      _state_delay_timer = 60;
      return;
    }
  }

  float g_dx = _ball_x - _goal_pos.x;
  float g_dy = _ball_y - _goal_pos.y;
  float g_distance = std::sqrt(g_dx * g_dx + g_dy * g_dy);
  
  if (g_distance < (LABY_GRID_SIZE / 2)) {
    _game_state = LabyState::VICTORY;
    _state_delay_timer = 120;
    
    // 🚀 FIXED: Evaluate multi-level completion sequence milestones
    if (_current_level + 1 >= LABY_MAX_LEVELS) {
      if (_sensor_suite) {
        _sensor_suite->save_state.unlock("Balancer");
      }
      _create_popup_overlay("VICTORY!\nALL MAZES CONQUERED.");
    } else {
      char status_msg[32];
      snprintf(status_msg, sizeof(status_msg), "ROOM %d CLEAR!\nLoading next...", _current_level + 1);
      _create_popup_overlay(status_msg);
    }
  }
}

ScreenAction LabyrinthGame::update() {
  _update_action.led_upper_func = &Charlieplex::animation_off;
  _update_action.led_lower_func = &Charlieplex::animation_off;

  if (_game_state == LabyState::GAMEPLAY) {
    _process_physics();
  } else if (_game_state == LabyState::HOLE_FALL) {
    if (_state_delay_timer > 0) {
      _state_delay_timer--;
    } else {
      // 🚀 FIXED: Restarting current level. Keep index, just clear positional coordinates.
      _parse_map(); 
      _reset_ball();
      _game_state = LabyState::GAMEPLAY;
    }
  } else if (_game_state == LabyState::VICTORY) {
    if (_state_delay_timer > 0) {
      _state_delay_timer--;
    } else {
      // 🚀 FIXED: Step forward to next layout space map configuration sequentially
      if (_current_level + 1 < LABY_MAX_LEVELS) {
        _current_level++;
        _parse_map();
        _reset_ball();
        _game_state = LabyState::GAMEPLAY;
      } else {
        // All levels completed. Return cleanly to the main UI stack container
        _update_action.type = ScreenActionType::PUSH_SUBMENU;
        _update_action.next_screen = _screen_stack.empty() ? nullptr : _screen_stack.front();
      }
    }
  }

  lv_obj_invalidate(_game_container);
  return Game::update();
}

void LabyrinthGame::_game_key_cb(lv_event_t* e) {
  LabyrinthGame* instance = (LabyrinthGame*)lv_event_get_user_data(e);
  uint32_t key = lv_event_get_key(e);

  if (key == LV_KEY_HOME) {
    instance->_update_action.type = ScreenActionType::PUSH_SUBMENU;
    instance->_update_action.next_screen = instance->_screen_stack.empty() ? nullptr : instance->_screen_stack.front();
  }
}

void LabyrinthGame::_game_draw_cb(lv_event_t* e) {
  lv_layer_t* layer = lv_event_get_layer(e);
  LabyrinthGame* instance = (LabyrinthGame*)lv_event_get_user_data(e);
  lv_obj_t* obj = (lv_obj_t*)lv_event_get_target(e);
  lv_area_t container_coords;
  lv_obj_get_coords(obj, &container_coords);

  lv_draw_rect_dsc_t rect_dsc;
  lv_draw_rect_dsc_init(&rect_dsc);
  rect_dsc.bg_opa = LV_OPA_COVER;

  // 1. Draw Static Maze Map Grid Configuration Structures matching active current room
  for (int8_t r = 0; r < LABY_GRID_ROWS; r++) {
    for (int8_t c = 0; c < LABY_GRID_COLS; c++) {
      uint8_t cell = instance->_maps[instance->_current_level][r][c];
      if (cell == (uint8_t)CellType::EMPTY || cell == (uint8_t)CellType::START) continue;

      lv_area_t cell_area;
      cell_area.x1 = container_coords.x1+c * LABY_GRID_SIZE;
      cell_area.y1 = container_coords.y1+r * LABY_GRID_SIZE;
      cell_area.x2 = cell_area.x1 + LABY_GRID_SIZE - 1;
      cell_area.y2 = cell_area.y1 + LABY_GRID_SIZE - 1;

      if (cell == (uint8_t)CellType::WALL) {
        rect_dsc.bg_color = lv_color_make(100, 100, 100); // Gray Walls
        lv_draw_rect(layer, &rect_dsc, &cell_area);
      } else if (cell == (uint8_t)CellType::HOLE) {
        rect_dsc.radius = LV_RADIUS_CIRCLE; 
        rect_dsc.bg_color = lv_color_make(90, 90, 90); // Dark Crimson Warning Danger Holes
        lv_draw_rect(layer, &rect_dsc, &cell_area);
        rect_dsc.radius = 0; 
      } else if (cell == (uint8_t)CellType::GOAL) {
        rect_dsc.bg_color = lv_color_make(255, 255, 255); // Vibrant Green Destination Goal
        lv_draw_rect(layer, &rect_dsc, &cell_area);
      }
    }
  } //  FIXED: Grid loop structural scope closes cleanly here

  // 2. Render the Active Moving Ball Element Mesh on screen layout
  lv_draw_rect_dsc_init(&rect_dsc);
  rect_dsc.bg_opa = LV_OPA_COVER;
  rect_dsc.bg_color = lv_color_make(255, 255, 255); // Golden yellow ball matrix element
  rect_dsc.radius = LV_RADIUS_CIRCLE;

  lv_area_t ball_area;
  ball_area.x1 = container_coords.x1+(int16_t)(instance->_ball_x - LABY_BALL_RADIUS);
  ball_area.y1 = container_coords.y1+(int16_t)(instance->_ball_y - LABY_BALL_RADIUS);
  ball_area.x2 = container_coords.x1+(int16_t)(instance->_ball_x + LABY_BALL_RADIUS);
  ball_area.y2 = container_coords.y1+(int16_t)(instance->_ball_y + LABY_BALL_RADIUS);

  lv_draw_rect(layer, &rect_dsc, &ball_area);
} 

void LabyrinthGame::end(bool is_leaving_upward) {
  if (is_leaving_upward) {
    if (_game_container) {
      lv_obj_del(_game_container);
      _game_container = nullptr;
    }
  } else {
    lv_obj_add_flag(_game_container, LV_OBJ_FLAG_HIDDEN);
  }
}