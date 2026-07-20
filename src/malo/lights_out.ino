#include "lights_out.h"

void LightsOut::begin(bool is_enter_from_above, SensorSuite *sensor_suite) {
    Game::begin(is_enter_from_above, sensor_suite);
    
    if (is_enter_from_above || _game_container == nullptr) {
        _is_won = false;
        _cursor_r = 0;
        _cursor_c = 0;
        _scramble_grid();

        // Attach UI layout box
        _game_container = lv_obj_create(lv_screen_active()); 
        lv_obj_set_size(_game_container, LO_SCREEN_WIDTH, LO_SCREEN_HEIGHT);
        lv_obj_align(_game_container, LV_ALIGN_TOP_LEFT, 0, HEADER_HEIGHT_PX);
        
        lv_obj_set_style_bg_color(_game_container, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(_game_container, LV_OPA_COVER, 0); 
        lv_obj_set_style_border_width(_game_container, 0, 0);
        lv_obj_set_style_pad_all(_game_container, 0, 0);
        lv_obj_remove_flag(_game_container, LV_OBJ_FLAG_SCROLLABLE);

        // Hook up processing pipelines
        lv_obj_add_event_cb(_game_container, _game_draw_cb, LV_EVENT_DRAW_MAIN, this);
        lv_obj_add_event_cb(_game_container, LightsOut::_game_key_cb, LV_EVENT_KEY, this);
    }

    lv_obj_clear_flag(_game_container, LV_OBJ_FLAG_HIDDEN);

    if (_input_group) { 
        lv_group_add_obj(_input_group, _game_container);
        lv_group_focus_obj(_game_container);
    }

    lv_obj_invalidate(_game_container);
}

void LightsOut::end(bool is_leaving_upward) {
    if (is_leaving_upward) {
        if (_game_container) {
            lv_obj_del(_game_container);
            _game_container = nullptr;
        }
    } else {
        if (_game_container) {
            lv_obj_add_flag(_game_container, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

ScreenAction LightsOut::update() {
    // Game updates immediately through event input handles, no active tickers required
  _update_action.led_upper_func = &Charlieplex::animation_off;
  _update_action.led_lower_func = &Charlieplex::animation_off;
    return Game::update();
}

void LightsOut::_scramble_grid() {
    // Generate a solvable arrangement where at least a few random outer cells are set to true (ON)
    // Make sure center cell is explicitly dead out of precautions
    for (uint8_t r = 0; r < LO_GRID_SIZE; r++) {
        for (uint8_t c = 0; c < LO_GRID_SIZE; c++) {
            _grid[r][c] = (random(0, 100) > 40); 
        }
    }
    _grid[1][1] = false; // Central cell locked to OFF/Blocked
}

void LightsOut::_move_cursor(int8_t dr, int8_t dc) {
    if (_is_won) return;

    int8_t target_r = _cursor_r + dr;
    int8_t target_c = _cursor_c + dc;

    // Check borders layout limits
    if (target_r < 0 || target_r >= LO_GRID_SIZE || target_c < 0 || target_c >= LO_GRID_SIZE) return;

    // Skip the central locked zone entirely by jumping over it
    if (target_r == 1 && target_c == 1) {
        target_r += dr;
        target_c += dc;
        if (target_r < 0 || target_r >= LO_GRID_SIZE || target_c < 0 || target_c >= LO_GRID_SIZE) return;
    }

    _cursor_r = target_r;
    _cursor_c = target_c;
    lv_obj_invalidate(_game_container);
}

void LightsOut::_toggle_cell(int8_t r, int8_t c) {
    // Bounds check and skip central index
    if (r < 0 || r >= LO_GRID_SIZE || c < 0 || c >= LO_GRID_SIZE) return;
    if (r == 1 && c == 1) return; 

    _grid[r][c] = !_grid[r][c];
}

void LightsOut::_handle_action() {
    if (_is_won) return;

    // RULE 1: User can only toggle a cell if it is currently ON.
    if (!_grid[_cursor_r][_cursor_c])
    {
      _create_popup_overlay("Turn lights off\nnot on!");
      //while(1){Serial.printf("light\n");delay(100);}
      return;
    }

    // Toggle current cell
    _toggle_cell(_cursor_r, _cursor_c);

    // RULE 2: Toggling a cell also toggles its neighboring 2 cells. 
    // In a 3x3 grid layout with the center missing, "neighbors" means adjacent horizontal/vertical items.
    _toggle_cell(_cursor_r - 1, _cursor_c); // Top
    _toggle_cell(_cursor_r + 1, _cursor_c); // Bottom
    _toggle_cell(_cursor_r, _cursor_c - 1); // Left
    _toggle_cell(_cursor_r, _cursor_c + 1); // Right

    // Evaluate victory sequence rules
    if (_check_victory()) {
        _is_won = true;
        if (_sensor_suite!=nullptr) {
            _sensor_suite->save_state.unlock("Dark MalO Rises");
        }
        _create_popup_overlay("You win!");
    }
    
    lv_obj_invalidate(_game_container);
}

bool LightsOut::_check_victory() {
    for (uint8_t r = 0; r < LO_GRID_SIZE; r++) {
        for (uint8_t c = 0; c < LO_GRID_SIZE; c++) {
            if (r == 1 && c == 1) continue; // Ignore center
            if (_grid[r][c]) return false;  // Found an active light
        }
    }
    return true;
}

void LightsOut::_game_key_cb(lv_event_t* e) {
    LightsOut* game = static_cast<LightsOut*>(lv_event_get_user_data(e));
    uint32_t key = lv_event_get_key(e);

    switch(key) {
        case LV_KEY_UP:    game->_move_cursor(-1, 0);  break;
        case LV_KEY_DOWN:  game->_move_cursor(1, 0);   break;
        case LV_KEY_LEFT:  game->_move_cursor(0, -1);  break;
        case LV_KEY_RIGHT: game->_move_cursor(0, 1);   break;
        case LV_KEY_ENTER: game->_handle_action();     if(game->_is_won){ game->_is_won = false; game->_cursor_r = 0; game->_cursor_c = 0; game->_scramble_grid(); } break;
        case LV_KEY_ESC:                               if(game->_is_won){ game->_is_won = false; game->_cursor_r = 0; game->_cursor_c = 0; game->_scramble_grid(); } break;
        case LV_KEY_HOME:
            game->_update_action.type = ScreenActionType::PUSH_SUBMENU;
            game->_update_action.next_screen = game->_screen_stack.empty() ? nullptr : game->_screen_stack.front();
          break;
    }
}

void LightsOut::_game_draw_cb(lv_event_t* e) {
    lv_layer_t* layer = lv_event_get_layer(e);
    LightsOut* game = static_cast<LightsOut*>(lv_event_get_user_data(e));
    lv_obj_t* obj = (lv_obj_t*)lv_event_get_target(e);
    lv_area_t container_coords;
    lv_obj_get_coords(obj, &container_coords);

    // Center layout mathematically on the active 320x240 frame space
    int16_t start_x = (LO_SCREEN_WIDTH - (LO_GRID_SIZE * LO_CELL_DIM)) / 2;
    int16_t start_y = (LO_SCREEN_HEIGHT - (LO_GRID_SIZE * LO_CELL_DIM)) / 2;

    lv_draw_rect_dsc_t rect_dsc;
    lv_draw_rect_dsc_init(&rect_dsc);
    rect_dsc.border_width = 2;
    rect_dsc.border_color = lv_color_make(60, 60, 60);

    lv_draw_line_dsc_t line_dsc;
    lv_draw_line_dsc_init(&line_dsc);
    line_dsc.color = lv_color_make(180, 0, 0); // Dark Crimson/Red for structural block indicator
    line_dsc.width = 3;

    for (int8_t r = 0; r < LO_GRID_SIZE; r++) {
        for (int8_t c = 0; c < LO_GRID_SIZE; c++) {
            lv_area_t cell_area;
            cell_area.x1 = container_coords.x1 + start_x + (c * LO_CELL_DIM);
            cell_area.y1 = container_coords.y1 + start_y + (r * LO_CELL_DIM);
            cell_area.x2 = cell_area.x1 + LO_CELL_DIM - 4;
            cell_area.y2 = cell_area.y1 + LO_CELL_DIM - 4;

            // RULE 1: Center cell (1,1) is permanently locked and NEVER selectable
            if (r == 1 && c == 1) {
                rect_dsc.bg_color = lv_color_make(0, 0, 0);
                rect_dsc.border_color = lv_color_make(0, 0, 0); // Keep normal border
                rect_dsc.border_width = 2;
                lv_draw_rect(layer, &rect_dsc, &cell_area);
                
                line_dsc.p1 = { (lv_value_precise_t)cell_area.x1, (lv_value_precise_t)cell_area.y1 };
                line_dsc.p2 = { (lv_value_precise_t)cell_area.x2, (lv_value_precise_t)cell_area.y2 };
                lv_draw_line(layer, &line_dsc);

                line_dsc.p1 = { (lv_value_precise_t)cell_area.x2, (lv_value_precise_t)cell_area.y1 };
                line_dsc.p2 = { (lv_value_precise_t)cell_area.x1, (lv_value_precise_t)cell_area.y2 };
                lv_draw_line(layer, &line_dsc);
                continue;
            }

            // Define light states colors
            if (game->_grid[r][c]) {
                rect_dsc.bg_color = lv_color_make(100, 100, 100); // ON state
            } else {
                rect_dsc.bg_color = lv_color_make(0, 0, 0);  // OFF state
            }

            // RULE 2: Only show the cursor itself
            if (!game->_is_won) {
                if (game->_cursor_r == r && game->_cursor_c == c) {
                    // Current primary cell under cursor gets a thick, bright white border
                    rect_dsc.border_color = lv_color_make(255, 255, 255); 
                    rect_dsc.border_width = 4;
                } else {
                    //  FIXED: Neighbors now fall through here, keeping normal unselected borders
                    rect_dsc.border_color = lv_color_make(0, 0, 0);
                    rect_dsc.border_width = 2;
                }
            } else {
                // Game is won, drop all cursor borders to default
                rect_dsc.border_color = lv_color_make(0, 0, 0);
                rect_dsc.border_width = 2;
            }

            lv_draw_rect(layer, &rect_dsc, &cell_area);
        }
    }
}