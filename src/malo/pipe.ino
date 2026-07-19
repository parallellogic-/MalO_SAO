#include "pipe.h"
#include <cstdlib>
#include <cmath>

PipeGame::PipeGame(const std::string& text, lv_group_t* shared_input_group) : Game(text, shared_input_group) {}

void PipeGame::begin(bool is_enter_from_above, SensorSuite *sensor_suite) {
    Game::begin(is_enter_from_above, sensor_suite);
    _sensors = sensor_suite;
    
    if (is_enter_from_above) {
        _overlay_card = nullptr;
        _overlay_timer = nullptr;

        _game_container = lv_obj_create(lv_screen_active()); 
        lv_obj_set_size(_game_container, PIPE_SCREEN_WIDTH, PIPE_SCREEN_HEIGHT);
        lv_obj_align(_game_container, LV_ALIGN_TOP_LEFT, 0, HEADER_HEIGHT_PX);
        
        lv_obj_set_style_bg_color(_game_container, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(_game_container, LV_OPA_COVER, 0); 
        lv_obj_set_style_border_width(_game_container, 0, 0);
        lv_obj_set_style_pad_all(_game_container, 0, 0);
        lv_obj_remove_flag(_game_container, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_add_event_cb(_game_container, _game_draw_cb, LV_EVENT_DRAW_MAIN, this);
        lv_obj_add_event_cb(_game_container, PipeGame::_game_key_cb, LV_EVENT_KEY, this);

        _generate_solvable_board();
    } else {
        lv_obj_clear_flag(_game_container, LV_OBJ_FLAG_HIDDEN);
    }

    if (_input_group) { 
        lv_group_add_obj(_input_group, _game_container);
        lv_group_focus_obj(_game_container);
    }
    lv_obj_invalidate(_game_container);
}

void PipeGame::_generate_solvable_board() {
    _game_state = PipeState::GAMEPLAY;

    // Step 1: Initialize an empty/basic board configuration
    for (int y = 0; y < PIPE_GRID_ROWS; y++) {
        for (int x = 0; x < PIPE_GRID_COLS; x++) {
            _grid[y][x] = { PipeType::STRAIGHT, 0, false, 1.0f };
        }
    }

    // Step 2: Carve a definitive working path from (0,0) to exit (ROWS-1, COLS-1)
    int8_t cur_x = 0, cur_y = 0;
    int8_t last_dir = 1; // Started moving Right
    _grid[0][0].type = PipeType::BEND; // Force curve out from edge entry

    while (cur_x < PIPE_GRID_COLS - 1 || cur_y < PIPE_GRID_ROWS - 1) {
        int8_t next_x = cur_x;
        int8_t next_y = cur_y;
        
        // Naive random step forward prioritizing destination direction
        if ((std::rand() % 2 == 0 && cur_x < PIPE_GRID_COLS - 1) || cur_y == PIPE_GRID_ROWS - 1) {
            next_x++;
        } else {
            next_y++;
        }

        // Determine junction styling needed to bridge the steps seamlessly
        if (next_x > cur_x) { // Stepping Right
            if (last_dir == 2) _grid[cur_y][cur_x].type = PipeType::BEND; // Came down, turning right
            else _grid[cur_y][cur_x].type = PipeType::STRAIGHT;
            last_dir = 1;
        } else if (next_y > cur_y) { // Stepping Down
            if (last_dir == 1) _grid[cur_y][cur_x].type = PipeType::BEND; // Came right, turning down
            else _grid[cur_y][cur_x].type = PipeType::STRAIGHT;
            last_dir = 2;
        }
        
        cur_x = next_x;
        cur_y = next_y;
    }
    // Set exit pipe styling
    _grid[PIPE_GRID_ROWS-1][PIPE_GRID_COLS-1].type = (last_dir == 1) ? PipeType::STRAIGHT : PipeType::BEND;

    // Step 3: Populate remaining grid tiles with chaotic variety
    for (int y = 0; y < PIPE_GRID_ROWS; y++) {
        for (int x = 0; x < PIPE_GRID_COLS; x++) {
            if (std::rand() % 5 == 0) { // Spice path up with intersection types
                _grid[y][x].type = (std::rand() % 2 == 0) ? PipeType::THREE_WAY : PipeType::FOUR_WAY;
            }
        }
    }

    // Step 4: Scramble using matching cross cursor mechanics to preserve solvability constraints
    for (int i = 0; i < 30; i++) {
        _cursor_x = 1 + (std::rand() % (PIPE_GRID_COLS - 2));
        _cursor_y = 1 + (std::rand() % (PIPE_GRID_ROWS - 2));
        //if(_cursor_x>2 && _cursor_y>2) _rotate_cursor(this,std::rand() % 2 == 0); //only rotate away from starting area
    }

    // Reset baseline interactive cursor placement
    _cursor_x = PIPE_GRID_COLS / 2;
    _cursor_y = PIPE_GRID_ROWS / 2;

    // Calculate instant fluid routing state for the initial scramble layout
    _process_fluid_simulation();
}

void PipeGame::_rotate_piece(PipeGame* self, int8_t y, int8_t x, bool clockwise) {
    if (x < 0 || x >= PIPE_GRID_COLS || y < 0 || y >= PIPE_GRID_ROWS) return;

    if (clockwise) {
        self->_grid[y][x].rotation = (self->_grid[y][x].rotation + 1) % 4;
    } else {
        self->_grid[y][x].rotation = (self->_grid[y][x].rotation + 3) % 4;
    }
}

void PipeGame::_rotate_cursor(PipeGame* self, bool clockwise) {
    // FIXED: Now passes the active interactive object layer explicitly
    _rotate_piece(self, self->_cursor_y, self->_cursor_x, clockwise);       // Center
    _rotate_piece(self, self->_cursor_y - 1, self->_cursor_x, clockwise);   // Up
    _rotate_piece(self, self->_cursor_y + 1, self->_cursor_x, clockwise);   // Down
    _rotate_piece(self, self->_cursor_y, self->_cursor_x - 1, clockwise);   // Left
    _rotate_piece(self, self->_cursor_y, self->_cursor_x + 1, clockwise);   // Right

    // Run structural tracer logic on the targeted instance matrix
    self->_process_fluid_simulation();
}

bool PipeGame::_get_connections(int8_t y, int8_t x, bool connections[4]) {
    for(int i=0; i<4; ++i) connections[i] = false;
    if (x < 0 || x >= PIPE_GRID_COLS || y < 0 || y >= PIPE_GRID_ROWS) return false;

    PipePiece p = _grid[y][x];
    bool local[4] = {false};

    switch (p.type) {
        case PipeType::STRAIGHT:
            local[1] = true; local[3] = true; // Horizontal baseline
            break;
        case PipeType::BEND:
            local[0] = true; local[1] = true; // Top-to-Right curve baseline
            break;
        case PipeType::THREE_WAY:
            local[0] = true; local[1] = true; local[3] = true; // T configuration
            break;
        case PipeType::FOUR_WAY:
            local[0] = true; local[1] = true; local[2] = true; local[3] = true;
            break;
    }

    // Apply native rotation state alignment modifications
    for (int i = 0; i < 4; i++) {
        connections[(i + p.rotation) % 4] = local[i];
    }
    return true;
}

void PipeGame::_process_fluid_simulation() {
    // Wipe old fluid tracing state entirely before calculating current route map
    for (int y = 0; y < PIPE_GRID_ROWS; y++) {
        for (int x = 0; x < PIPE_GRID_COLS; x++) {
            _grid[y][x].filled = false;
        }
    }

    // Start instantaneous trajectory simulation from top-left cell
    int8_t cx = 0;
    int8_t cy = 0;
    int8_t edir = 3; // Liquid structural entry index from left margin bounds
    bool visited[PIPE_GRID_ROWS][PIPE_GRID_COLS] = {false};

    while (cx >= 0 && cx < PIPE_GRID_COLS && cy >= 0 && cy < PIPE_GRID_ROWS) {
        if (visited[cy][cx]) break; // Closed tracking loop detected
        visited[cy][cx] = true;

        bool conn[4];
        _get_connections(cy, cx, conn);
        if (!conn[edir]) break; // Connection line breakdown mismatch

        // Instantly mark the reached segment as illuminated path fluid
        _grid[cy][cx].filled = true;

        // Win condition evaluation check
        if (cx == PIPE_GRID_COLS - 1 && cy == PIPE_GRID_ROWS - 1) {
            _game_state = PipeState::WIN;
            if (_sensors) {
                _sensors->save_state.unlock("Plumber");
            }
            return;
        }

        // Search out alternative connection routing paths
        int8_t out_dir = -1;
        for (int d = 0; d < 4; d++) {
            if (d != edir && conn[d]) {
                out_dir = d;
                break;
            }
        }
        if (out_dir == -1) break; // Dead end branch configuration

        // Transition step into neighbor coordinates
        int8_t next_x = cx;
        int8_t next_y = cy;
        int8_t next_entry = -1;

        if (out_dir == 0) { next_y--; next_entry = 2; }
        else if (out_dir == 1) { next_x++; next_entry = 3; }
        else if (out_dir == 2) { next_y++; next_entry = 0; }
        else if (out_dir == 3) { next_x--; next_entry = 1; }

        // Test neighbor pipe alignment connection points
        bool next_conn[4];
        if (_get_connections(next_y, next_x, next_conn) && next_conn[next_entry]) {
            cx = next_x;
            cy = next_y;
            edir = next_entry;
        } else {
            break; // Misaligned entry segment boundary
        }
    }
}

ScreenAction PipeGame::update() {
    // Game updates immediately through event input handles, no active tickers required
    return Game::update();
}

void PipeGame::_game_key_cb(lv_event_t* e) {
    PipeGame* self = (PipeGame*)lv_event_get_user_data(e);
    uint32_t key = lv_event_get_key(e);

    if (self->_game_state == PipeState::WIN) {
        if (key == LV_KEY_ENTER) self->_generate_solvable_board();
        if (key == LV_KEY_HOME) {
            self->_update_action.type = ScreenActionType::PUSH_SUBMENU;
            self->_update_action.next_screen = self->_screen_stack.empty() ? nullptr : self->_screen_stack.front();
        }
        return;
    }

    switch (key) {
        case LV_KEY_UP:    if (self->_cursor_y > 0) self->_cursor_y--; break;
        case LV_KEY_DOWN:  if (self->_cursor_y < PIPE_GRID_ROWS - 1) self->_cursor_y++; break;
        case LV_KEY_LEFT:  if (self->_cursor_x > 0) self->_cursor_x--; break;
        case LV_KEY_RIGHT: if (self->_cursor_x < PIPE_GRID_COLS - 1) self->_cursor_x++; break;
        case (lv_key_t)'/':
        case LV_KEY_ENTER: self->_rotate_cursor(self, true); break;  // FIXED: Passes self pointer
        case (lv_key_t)';':  self->_rotate_cursor(self, false); break; // FIXED: Passes self pointer
        case LV_KEY_HOME:
            self->_update_action.type = ScreenActionType::PUSH_SUBMENU;
            self->_update_action.next_screen = self->_screen_stack.empty() ? nullptr : self->_screen_stack.front();
            break;
    }
    lv_obj_invalidate(self->_game_container);
}


void PipeGame::_game_draw_cb(lv_event_t* e) 
{
    PipeGame* self = (PipeGame*)lv_event_get_user_data(e);
    lv_layer_t* layer = lv_event_get_layer(e);
    
    // Fetch the active hardware clipping area boundaries
    const lv_area_t* clip_area = &layer->_clip_area;

    lv_draw_rect_dsc_t rect_dsc;
    lv_draw_rect_dsc_init(&rect_dsc);

    lv_draw_line_dsc_t line_dsc;
    lv_draw_line_dsc_init(&line_dsc);
    line_dsc.width = 6;

    // 1. Canvas Background
    lv_area_t bg_area = {0, HEADER_HEIGHT_PX, (int16_t)PIPE_SCREEN_WIDTH, (int16_t)SCREEN_HEIGHT_PX};
    
    // Using explicit bounding box overlap math to bypass internal macro redefinition bugs
    bool bg_visible = !(bg_area.x2 < clip_area->x1 || bg_area.x1 > clip_area->x2 ||
                        bg_area.y2 < clip_area->y1 || bg_area.y1 > clip_area->y2);

    if (bg_visible) {
        rect_dsc.bg_color = lv_color_make(0, 0, 0);
        rect_dsc.border_width = 0;
        lv_draw_rect(layer, &rect_dsc, &bg_area);
    }

    // Pre-calculate half size to remove division inside the loop
    const int16_t half_cell = PIPE_CELL_SIZE / 2;

    // 2. Draw the grid system and pipes
    for (int y = 0; y < PIPE_GRID_ROWS; y++) {
        int16_t cell_y = HEADER_HEIGHT_PX + (y * PIPE_CELL_SIZE);
        
        // Horizontal scan check: Skip the whole row if out of vertical clip scope
        if (cell_y > clip_area->y2 || (cell_y + PIPE_CELL_SIZE) < clip_area->y1) {
            continue;
        }

        for (int x = 0; x < PIPE_GRID_COLS; x++) {
            int16_t cell_x = x * PIPE_CELL_SIZE;

            // Cell boundary tracking box
            int16_t cell_x2 = cell_x + PIPE_CELL_SIZE;
            int16_t cell_y2 = cell_y + PIPE_CELL_SIZE;

            // Direct hardware check: skip calculations if cell falls entirely off-tile
            if (cell_x2 < clip_area->x1 || cell_x > clip_area->x2 ||
                cell_y2 < clip_area->y1 || cell_y > clip_area->y2) {
                continue;
            }

            int16_t center_x = cell_x + half_cell;
            int16_t center_y = cell_y + half_cell;

            // Draw bounding boxes around items
            rect_dsc.bg_color = lv_color_make(35, 35, 40);
            rect_dsc.border_color = lv_color_make(50, 50, 55);
            rect_dsc.border_width = 1;
            rect_dsc.bg_opa = LV_OPA_COVER; 
            
            lv_area_t cell_area = {cell_x, cell_y, cell_x2, cell_y2};
            lv_draw_rect(layer, &rect_dsc, &cell_area);

            // FIXED: Properly dimensioned array to match _get_connections signature
            bool conn[4];
            self->_get_connections(y, x, conn);
            line_dsc.color = (self->_grid[y][x].filled) ? lv_color_make(255, 255, 255) : lv_color_make(90, 90, 90);

            // Center core node mapping
            for (int d = 0; d < 4; d++) {
                if (conn[d]) { // FIXED: conn is an array again
                    line_dsc.p1.x = center_x;
                    line_dsc.p1.y = center_y;
                    line_dsc.p2.x = center_x;
                    line_dsc.p2.y = center_y;

                    if (d == 0) line_dsc.p2.y -= half_cell;
                    if (d == 1) line_dsc.p2.x += half_cell;
                    if (d == 2) line_dsc.p2.y += half_cell;
                    if (d == 3) line_dsc.p2.x -= half_cell;

                    lv_draw_line(layer, &line_dsc);
                }
            }
        }
    }

    // 3. Render Cross Selector Cursor Overlay Highlight
    if (self->_game_state == PipeState::GAMEPLAY) {
        rect_dsc.bg_opa = LV_OPA_TRANSP;
        rect_dsc.border_color = lv_color_make(255, 200, 0);
        rect_dsc.border_width = 2;

        // FIXED: Properly dimensioned matrix structure
        int8_t offsets[5][2] = {{0,0}, {-1,0}, {1,0}, {0,-1}, {0,1}};
        for (int i = 0; i < 5; i++) {
            // FIXED: Safely pull row offset from entry column 0, column offset from entry column 1
            int8_t ny = self->_cursor_y + offsets[i][0];
            int8_t nx = self->_cursor_x + offsets[i][1];

            if (nx >= 0 && nx < PIPE_GRID_COLS && ny >= 0 && ny < PIPE_GRID_ROWS) {
                int16_t cx = nx * PIPE_CELL_SIZE;
                int16_t cy = HEADER_HEIGHT_PX + (ny * PIPE_CELL_SIZE);
                int16_t cx2 = cx + PIPE_CELL_SIZE;
                int16_t cy2 = cy + PIPE_CELL_SIZE;

                if (!(cx2 < clip_area->x1 || cx > clip_area->x2 ||
                    cy2 < clip_area->y1 || cy > clip_area->y2)) {
                    lv_area_t cursor_area = {cx, cy, cx2, cy2};
                    lv_draw_rect(layer, &rect_dsc, &cursor_area);
                }
            }
        }
    }

    // 4. State Status Alerts UI Overlays
    if (self->_game_state == PipeState::WIN && bg_visible) {
        rect_dsc.bg_color = lv_color_black();
        rect_dsc.bg_opa = LV_OPA_70;
        lv_draw_rect(layer, &rect_dsc, &bg_area);
    }
}


void PipeGame::end(bool is_leaving_upward) 
{
    if (is_leaving_upward) {
        if (_game_container) {
            lv_obj_del(_game_container);
            _game_container = nullptr;
        }
    } else {
        lv_obj_add_flag(_game_container, LV_OBJ_FLAG_HIDDEN);
    }
}