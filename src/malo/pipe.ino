#include "pipe.h"
#include <cstdlib>
#include <cmath>

PipeGame::PipeGame(const std::string& text, lv_group_t* shared_input_group) : Game(text, shared_input_group) {}

void PipeGame::begin(bool is_enter_from_above, SensorSuite *sensor_suite) {
    Game::begin(is_enter_from_above, sensor_suite);
    
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
        _create_popup_overlay("Rotate!");
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
    int8_t first_step_dir = -1; // Track which direction the first step took

    while (cur_x < PIPE_GRID_COLS - 1 || cur_y < PIPE_GRID_ROWS - 1) {
        int8_t next_x = cur_x;
        int8_t next_y = cur_y;
        
        // Naive random step forward prioritizing destination direction
        if ((std::rand() % 2 == 0 && cur_x < PIPE_GRID_COLS - 1) || cur_y == PIPE_GRID_ROWS - 1) {
            next_x++;
        } else {
            next_y++;
        }

        // Capture direction of first departure out of (0,0)
        if (cur_x == 0 && cur_y == 0) {
            first_step_dir = (next_x > 0) ? 1 : 2; // 1: Right, 2: Down
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

    // --- FIX: Configure top-left cell (0,0) to reliably connect to the left border entrance (Port index 3) ---
    if (first_step_dir == 1) {
        // Path went Right: Straight horizontal line open Left (3) & Right (1)
        _grid[0][0].type = PipeType::STRAIGHT; 
        _grid[0][0].rotation = 0; // Default STRAIGHT connects ports 1 & 3
    } else {
        // Path went Down: Bend configuration curving open Left (3) & Down (2)
        _grid[0][0].type = PipeType::BEND;
        _grid[0][0].rotation = 1; // BEND(0) is Top/Right. Rotation 1 CW makes it Right/Bottom. Rotation 2 CW makes it Bottom/Left.
        _grid[0][0].rotation = 2; // Port 2 (Down) and Port 3 (Left)
    }

    // --- FIX: Configure bottom-right cell to reliably connect to the right border goal exit (Port index 1) ---
    int8_t ey = PIPE_GRID_ROWS - 1;
    int8_t ex = PIPE_GRID_COLS - 1;
    if (last_dir == 1) {
        // Path approached from the Left: Straight horizontal line open Left (3) & Right (1)
        _grid[ey][ex].type = PipeType::STRAIGHT;
        _grid[ey][ex].rotation = 0;
    } else {
        // Path approached from Upwards: Bend configuration curving open Up (0) & Right (1)
        _grid[ey][ex].type = PipeType::BEND;
        _grid[ey][ex].rotation = 0; // Default BEND connects Port 0 (Up) and Port 1 (Right)
    }

    // Step 3: Populate remaining grid tiles with chaotic variety
    for (int y = 0; y < PIPE_GRID_ROWS; y++) {
        for (int x = 0; x < PIPE_GRID_COLS; x++) {
            // Avoid modifying start and end pieces
            if ((x == 0 && y == 0) || (x == PIPE_GRID_COLS - 1 && y == PIPE_GRID_ROWS - 1)) {
                continue;
            }
            if (std::rand() % 5 == 0) { // Spice path up with intersection types
                _grid[y][x].type = (std::rand() % 2 == 0) ? PipeType::THREE_WAY : PipeType::FOUR_WAY;
            }
        }
    }

    // Step 4: Scramble using matching cross cursor mechanics to preserve solvability constraints
    for (int i = 0; i < 30; i++) {
        _cursor_x = 1 + (std::rand() % (PIPE_GRID_COLS - 2));
        _cursor_y = 1 + (std::rand() % (PIPE_GRID_ROWS - 2));
        _rotate_cursor(this, std::rand() % 2 == 0); 
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
    // 1. Clear previous fluid state completely
    for (int y = 0; y < PIPE_GRID_ROWS; y++) {
        for (int x = 0; x < PIPE_GRID_COLS; x++) {
            _grid[y][x].filled = false;
        }
    }

    struct FluidNode {
        int8_t x;
        int8_t y;
    };

    // Allocate queues and visited matrices for both paths
    FluidNode queue_start[PIPE_GRID_ROWS * PIPE_GRID_COLS];
    FluidNode queue_end[PIPE_GRID_ROWS * PIPE_GRID_COLS];
    int head_start = 0, tail_start = 0;
    int head_end = 0, tail_end = 0;

    bool visited_from_start[PIPE_GRID_ROWS][PIPE_GRID_COLS] = {false};
    bool visited_from_end[PIPE_GRID_ROWS][PIPE_GRID_COLS] = {false};

    // Neighbor mapping configuration (0: Up, 1: Right, 2: Down, 3: Left)
    int8_t dx[4] = {0, 1, 0, -1};
    int8_t dy[4] = {-1, 0, 1, 0};
    int8_t opposite_dir[4] = {2, 3, 0, 1};

    // --- PASS 1: Seed and Flood-Fill from the Start (Top-Left) ---
    bool start_conn[4];
    _get_connections(0, 0, start_conn);
    if (start_conn[3]) { // Feeds from Left border
        _grid[0][0].filled = true;
        visited_from_start[0][0] = true;
        queue_start[tail_start++] = {0, 0};
    }

    while (head_start < tail_start) {
        FluidNode curr = queue_start[head_start++];
        bool current_connections[4];
        _get_connections(curr.y, curr.x, current_connections);

        for (int d = 0; d < 4; d++) {
            if (!current_connections[d]) continue;
            int8_t nx = curr.x + dx[d];
            int8_t ny = curr.y + dy[d];

            if (nx >= 0 && nx < PIPE_GRID_COLS && ny >= 0 && ny < PIPE_GRID_ROWS) {
                if (!visited_from_start[ny][nx]) {
                    bool neighbor_connections[4];
                    _get_connections(ny, nx, neighbor_connections);
                    if (neighbor_connections[opposite_dir[d]]) {
                        _grid[ny][nx].filled = true;
                        visited_from_start[ny][nx] = true;
                        queue_start[tail_start++] = {nx, ny};
                    }
                }
            }
        }
    }

    // --- PASS 2: Seed and Flood-Fill from the End (Bottom-Right) ---
    int8_t ey = PIPE_GRID_ROWS - 1;
    int8_t ex = PIPE_GRID_COLS - 1;
    bool end_conn[4];
    _get_connections(ey, ex, end_conn);
    if (end_conn[1]) { // Exits right border
        _grid[ey][ex].filled = true;
        visited_from_end[ey][ex] = true;
        queue_end[tail_end++] = {ex, ey};
    }

    while (head_end < tail_end) {
        FluidNode curr = queue_end[head_end++];
        bool current_connections[4];
        _get_connections(curr.y, curr.x, current_connections);

        for (int d = 0; d < 4; d++) {
            if (!current_connections[d]) continue;
            int8_t nx = curr.x + dx[d];
            int8_t ny = curr.y + dy[d];

            if (nx >= 0 && nx < PIPE_GRID_COLS && ny >= 0 && ny < PIPE_GRID_ROWS) {
                if (!visited_from_end[ny][nx]) {
                    bool neighbor_connections[4];
                    _get_connections(ny, nx, neighbor_connections);
                    if (neighbor_connections[opposite_dir[d]]) {
                        _grid[ny][nx].filled = true;
                        visited_from_end[ny][nx] = true;
                        queue_end[tail_end++] = {nx, ny};
                    }
                }
            }
        }
    }

    // --- Clean Win Evaluation Check ---
    // The player wins if ANY cell reached by the start path directly connects to a cell reached by the end path.
    // This allows branches to link up perfectly anywhere across the layout.
    for (int y = 0; y < PIPE_GRID_ROWS; y++) {
        for (int x = 0; x < PIPE_GRID_COLS; x++) {
            if (!visited_from_start[y][x]) continue;

            bool current_connections[4];
            _get_connections(y, x, current_connections);

            for (int d = 0; d < 4; d++) {
                if (!current_connections[d]) continue;
                int8_t nx = x + dx[d];
                int8_t ny = y + dy[d];

                if (nx >= 0 && nx < PIPE_GRID_COLS && ny >= 0 && ny < PIPE_GRID_ROWS) {
                    // If the neighbor is part of the path linked to the end, check their physical joint connection
                    if (visited_from_end[ny][nx]) {
                        bool neighbor_connections[4];
                        _get_connections(ny, nx, neighbor_connections);
                        
                        if (neighbor_connections[opposite_dir[d]]) {
                            _game_state = PipeState::WIN;
                            if (_sensor_suite) {
                                _sensor_suite->save_state.unlock("Plumber");
                            }
                            return; // Path completed successfully
                        }
                    }
                }
            }
        }
    }
}



ScreenAction PipeGame::update() {
    // Game updates immediately through event input handles, no active tickers required
  _update_action.led_upper_func = &Charlieplex::animation_off;
  _update_action.led_lower_func = &Charlieplex::animation_off;
    return Game::update();
}

void PipeGame::_game_key_cb(lv_event_t* e) {
    PipeGame* self = (PipeGame*)lv_event_get_user_data(e);
    uint32_t key = lv_event_get_key(e);

    if (self->_game_state == PipeState::WIN) {
        if (key == LV_KEY_ENTER || key == LV_KEY_ESC) self->_generate_solvable_board();
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
            rect_dsc.bg_color = lv_color_make(0, 0, 0);
            rect_dsc.border_color = lv_color_make(50, 50, 50);
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
        rect_dsc.border_color = lv_color_make(255, 255, 255);
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
    /*if (self->_game_state == PipeState::WIN && bg_visible) {
        rect_dsc.bg_color = lv_color_black();
        rect_dsc.bg_opa = LV_OPA_70;
        lv_draw_rect(layer, &rect_dsc, &bg_area);
    }*/
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