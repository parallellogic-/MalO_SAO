#include "tictactoe.h"
#include <cstdlib> // For std::rand

TicTacToe::TicTacToe(const std::string& text, lv_group_t* shared_input_group):Game(text,shared_input_group){}

void TicTacToe::begin(bool is_enter_from_above,SensorSuite *sensor_suite)
{
  Game::begin(is_enter_from_above,sensor_suite);
  
  if(is_enter_from_above)
  {
    // Ensure pointers start cleanly on completely new allocations
    _overlay_card = nullptr;
    _overlay_timer = nullptr;

    // Test fix: Force parent container directly onto the active hardware viewport!
    _game_container = lv_obj_create(lv_screen_active()); 
    
    lv_obj_set_size(_game_container, SCREEN_WIDTH_PX, SCREEN_HEIGHT_PX - HEADER_HEIGHT_PX);
    lv_obj_align(_game_container, LV_ALIGN_TOP_LEFT, 0, HEADER_HEIGHT_PX);
    
    lv_obj_set_style_bg_color(_game_container, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(_game_container, LV_OPA_COVER, 0); 
    
    lv_obj_set_style_border_width(_game_container, 0, 0);
    lv_obj_set_style_pad_all(_game_container, 0, 0);
    lv_obj_remove_flag(_game_container, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(_game_container, LV_OBJ_FLAG_HIDDEN); //lv_obj_remove_flag

    lv_obj_add_event_cb(_game_container, _game_draw_cb, LV_EVENT_DRAW_MAIN, this); //LV_EVENT_DRAW_POST
    lv_obj_add_event_cb(_game_container, TicTacToe::_game_key_cb, LV_EVENT_KEY, this);

    _set_new_game();
  }else{
    lv_obj_clear_flag(_game_container, LV_OBJ_FLAG_HIDDEN);
    
  }

  if (_input_group) { 
      lv_group_add_obj(_input_group, _game_container);
      lv_group_focus_obj(_game_container);
  }

  lv_obj_invalidate(_game_container);
}

bool TicTacToe::_is_win()
{
    bool is_win = false;

    // The 8 possible winning line combinations on a 3x3 grid (indices 0-8)
    const uint8_t win_combinations[8][3] = {
        {0, 1, 2}, {3, 4, 5}, {6, 7, 8}, // Rows
        {0, 3, 6}, {1, 4, 7}, {2, 5, 8}, // Columns
        {0, 4, 8}, {2, 4, 6}             // Diagonals
    };

    for(uint8_t is_player = 0; is_player < 2; is_player++)
    {
        TicTacToePiece was_piece = is_player ? TicTacToePiece::PLAYER : TicTacToePiece::MALO;
        TicTacToePiece is_piece  = is_player ? TicTacToePiece::PLAYER_WINNER : TicTacToePiece::MALO_WINNER;

        // Iterate through all 8 possible winning lines
        for(uint8_t iter = 0; iter < 8; iter++)
        {
            uint8_t c1 = win_combinations[iter][0];
            uint8_t c2 = win_combinations[iter][1];
            uint8_t c3 = win_combinations[iter][2];

            // Check if all three cells match either the base piece or are already a winner
            bool match1 = (_board[c1] == was_piece || _board[c1] == is_piece);
            bool match2 = (_board[c2] == was_piece || _board[c2] == is_piece);
            bool match3 = (_board[c3] == was_piece || _board[c3] == is_piece);

            if(match1 && match2 && match3)
            {
                // Set all three to the winner state (keeps existing winners intact)
                _board[c1] = is_piece;
                _board[c2] = is_piece;
                _board[c3] = is_piece;
                
                is_win = true; 
                // Do not use 'break' here; continuing allows multi-direction wins to be updated!
            }
        }
    }

    return is_win;
}

#include <cstdlib> // For std::rand

uint8_t TicTacToe::_get_malo_move()
{
    // The 8 possible winning line combinations on a 3x3 grid
    const uint8_t win_combinations[8][3] = {
        {0, 1, 2}, {3, 4, 5}, {6, 7, 8}, // Rows
        {0, 3, 6}, {1, 4, 7}, {2, 5, 8}, // Columns
        {0, 4, 8}, {2, 4, 6}             // Diagonals
    };

    // -------------------------------------------------------------------------
    // RULE 1: Try to win directly (MALO has 2 pieces and 1 EMPTY spot in a line)
    // -------------------------------------------------------------------------
    for (uint8_t i = 0; i < 8; i++)
    {
        uint8_t c1 = win_combinations[i][0];
        uint8_t c2 = win_combinations[i][1];
        uint8_t c3 = win_combinations[i][2];

        uint8_t malo_count = 0;
        int8_t empty_index = -1;

        uint8_t cells[3] = {c1, c2, c3};
        for (uint8_t j = 0; j < 3; j++)
        {
            if (_board[cells[j]] == TicTacToePiece::MALO || _board[cells[j]] == TicTacToePiece::MALO_WINNER) {
                malo_count++;
            } else if (_board[cells[j]] == TicTacToePiece::EMPTY) {
                empty_index = cells[j];
            }
        }

        if (malo_count == 2 && empty_index != -1) {
            return (uint8_t)empty_index;
        }
    }

    // -------------------------------------------------------------------------
    // RULE 2: Try to block the player (PLAYER has 2 valid pieces and 1 EMPTY spot)
    // Only count player pieces where _count_down > 1
    // -------------------------------------------------------------------------
    for (uint8_t i = 0; i < 8; i++)
    {
        uint8_t c1 = win_combinations[i][0];
        uint8_t c2 = win_combinations[i][1];
        uint8_t c3 = win_combinations[i][2];

        uint8_t player_count = 0;
        int8_t empty_index = -1;

        uint8_t cells[3] = {c1, c2, c3};
        for (uint8_t j = 0; j < 3; j++)
        {
            bool is_player_piece = (_board[cells[j]] == TicTacToePiece::PLAYER || _board[cells[j]] == TicTacToePiece::PLAYER_WINNER);
            
            if (is_player_piece && _count_down[cells[j]] > 1) {
                player_count++;
            } else if (_board[cells[j]] == TicTacToePiece::EMPTY) {
                empty_index = cells[j];
            }
        }

        if (player_count == 2 && empty_index != -1) {
            return (uint8_t)empty_index;
        }
    }

    // -------------------------------------------------------------------------
    // RULE 3: Set up a future win (MALO has 1 piece and 2 EMPTY spots in a line)
    // -------------------------------------------------------------------------
    for (uint8_t i = 0; i < 8; i++)
    {
        uint8_t c1 = win_combinations[i][0];
        uint8_t c2 = win_combinations[i][1];
        uint8_t c3 = win_combinations[i][2];

        uint8_t malo_count = 0;
        uint8_t empty_count = 0;
        int8_t first_empty = -1;

        uint8_t cells[3] = {c1, c2, c3};
        for (uint8_t j = 0; j < 3; j++)
        {
            if (_board[cells[j]] == TicTacToePiece::MALO || _board[cells[j]] == TicTacToePiece::MALO_WINNER) {
                malo_count++;
            } else if (_board[cells[j]] == TicTacToePiece::EMPTY) {
                empty_count++;
                if (first_empty == -1) {
                    first_empty = cells[j];
                }
            }
        }

        if (malo_count == 1 && empty_count == 2) {
            return (uint8_t)first_empty;
        }
    }

    // -------------------------------------------------------------------------
    // RULE 4: Move randomly (Pick any available EMPTY square)
    // -------------------------------------------------------------------------
    uint8_t empty_cells[9];
    uint8_t empty_total = 0;

    for (uint8_t i = 0; i < 9; i++)
    {
        if (_board[i] == TicTacToePiece::EMPTY) {
            empty_cells[empty_total++] = i;
        }
    }

    if (empty_total > 0) {
        uint8_t random_choice = std::rand() % empty_total;
        return empty_cells[random_choice];
    }

    // Fallback error-safety (returns 0 if the board is fully jammed somehow)
    return 0;
}


void TicTacToe::_draw_piece(lv_layer_t* layer, lv_area_t& board_coords, TicTacToePiece piece, uint32_t frame_id, bool is_imminent_delete)
{
    lv_area_t cell_area;
    cell_area.x1 = board_coords.x1 + 4;
    cell_area.x2 = board_coords.x2 - 4;
    cell_area.y1 = board_coords.y1 + 4;
    cell_area.y2 = board_coords.y2 - 4;

    lv_color_t color=lv_color_hex(is_imminent_delete?0x505050:0xA0A0A0);
    if(piece == TicTacToePiece::PLAYER_CURSOR || piece == TicTacToePiece::PLAYER_WINNER || piece == TicTacToePiece::MALO_CURSOR || piece == TicTacToePiece::MALO_WINNER)
      color=lv_color_hex(frame_id % TICTACTOE_BLINK_PERIOD < (TICTACTOE_BLINK_PERIOD/2) ? 0x505050 : 0xFFFFFF); 

    // 1. Draw Player Pieces (X)
    if(piece == TicTacToePiece::PLAYER_CURSOR || piece == TicTacToePiece::PLAYER || piece == TicTacToePiece::PLAYER_WINNER)
    {
        lv_draw_line_dsc_t line_dsc;
        lv_draw_line_dsc_init(&line_dsc);
        line_dsc.width = 4; // Set a line width so the 'X' is visible
        line_dsc.color = color; 
        
        // First diagonal leg (\)
        line_dsc.p1.x = cell_area.x1;
        line_dsc.p1.y = cell_area.y1;
        line_dsc.p2.x = cell_area.x2;
        line_dsc.p2.y = cell_area.y2;
        lv_draw_line(layer, &line_dsc); 
        
        // Second diagonal leg (/)
        line_dsc.p1.x = cell_area.x1;
        line_dsc.p1.y = cell_area.y2;
        line_dsc.p2.x = cell_area.x2;
        line_dsc.p2.y = cell_area.y1;
        lv_draw_line(layer, &line_dsc);
    }
    
    // 2. Draw Opponent Pieces (O)
    if(piece == TicTacToePiece::MALO_CURSOR || piece == TicTacToePiece::MALO || piece == TicTacToePiece::MALO_WINNER)
    {
        lv_draw_rect_dsc_t rect_dsc;
        lv_draw_rect_dsc_init(&rect_dsc);
        
        rect_dsc.bg_opa = LV_OPA_TRANSP;       // Make the inside transparent
        rect_dsc.border_color = color;         // Apply your color to the outline
        rect_dsc.border_width = 4;             // Set the stroke thickness of the circle (matches X width)
        rect_dsc.radius = LV_RADIUS_CIRCLE;   // Keeps it a circle
        
        lv_draw_rect(layer, &rect_dsc, &cell_area);
    }
}

void TicTacToe::_game_draw_cb(lv_event_t* e) {
    lv_obj_t* obj = (lv_obj_t*)lv_event_get_target(e);
    TicTacToe* instance = (TicTacToe*)lv_event_get_user_data(e);
    if (!instance) return;
    
    lv_layer_t* layer = lv_event_get_layer(e);

    lv_area_t board_coords;
    lv_obj_get_coords(obj, &board_coords);
    
    int w = lv_area_get_width(&board_coords);
    int h = lv_area_get_height(&board_coords);
    int cell_w = w / 3;
    int cell_h = h / 3;

    // --- STEP 1: Draw Grid Lines (LVGL v9) ---
    lv_draw_line_dsc_t line_dsc;
    lv_draw_line_dsc_init(&line_dsc);
    line_dsc.color = lv_color_hex(0x505050); // Dim grey lines
    line_dsc.width = 2;

    for (int i = 1; i < 3; i++) {
        // --- Vertical Lines ---
        line_dsc.p1.x = board_coords.x1 + (i * cell_w);
        line_dsc.p1.y = board_coords.y1;
        line_dsc.p2.x = board_coords.x1 + (i * cell_w);
        line_dsc.p2.y = board_coords.y2;
        lv_draw_line(layer, &line_dsc); 
        
        // --- Horizontal Lines ---
        line_dsc.p1.x = board_coords.x1;
        line_dsc.p1.y = board_coords.y1 + (i * cell_h);
        line_dsc.p2.x = board_coords.x2;
        line_dsc.p2.y = board_coords.y1 + (i * cell_h);
        lv_draw_line(layer, &line_dsc); 
    }

    // --- STEP 2: Draw Pieces & Cursor Highlight ---
    lv_draw_rect_dsc_t rect_dsc;
    lv_draw_rect_dsc_init(&rect_dsc);
    
    for (int i = 0; i < 9; i++) {
        int row = i / 3;
        int col = i % 3;

        // Calculate current cell's raw bounding box bounds
        lv_area_t cell_area;
        cell_area.x1 = board_coords.x1 + (col * cell_w);
        cell_area.x2 = cell_area.x1 + cell_w;
        cell_area.y1 = board_coords.y1 + (row * cell_h);
        cell_area.y2 = cell_area.y1 + cell_h;


        // Draw Content symbols using the modular function
        if (instance->_board[i] != TicTacToePiece::EMPTY) {
            instance->_draw_piece(layer, cell_area, instance->_board[i],instance->_frame_id,instance->_count_down[i]<=1);
        }

        // Draw selection cursor highlight box
        if (i == instance->_cursor_index && (instance->_game_state==TicTacToePiece::PLAYER_CURSOR || instance->_game_state==TicTacToePiece::MALO_CURSOR)) {
            // Adjust box coordinates internally just for the cursor outline padding

            instance->_draw_piece(layer, cell_area, instance->_game_state,instance->_frame_id,instance->_count_down[i]<=1);
        }
    }
}

ScreenAction TicTacToe::update() {
    
    _update_action.led_upper_func=&Charlieplex::animation_off;
    _update_action.led_lower_func=&Charlieplex::animation_off;
    
    if(_frame_id%(TICTACTOE_BLINK_PERIOD/2)==0) lv_obj_invalidate(_game_container);//blink request for update

    if(_frame_id%(TICTACTOE_BLINK_PERIOD*5/2)==0)
    {//slowly animate malo moving cursor
      if(_game_state==TicTacToePiece::MALO_CURSOR && _malo_move<9)
      {
        uint8_t is_row = _cursor_index / 3;
        uint8_t is_col = _cursor_index % 3;
        uint8_t target_row = _malo_move / 3;
        uint8_t target_col = _malo_move % 3;
        if(is_col!=target_col)
        {
          if(is_col>target_col) _cursor_index--;
          else _cursor_index++;
        }else if(is_row!=target_row){
          if(is_row>target_row) _cursor_index-=3;
          else _cursor_index+=3;
        }else _make_move(); //if moved to location where target move is, make that move
      }
    }

//if malo move, try to win, try to block player from win, try to get 2 lined up (random selection of candidates), else random (ex on blank board)
//show animation to get to target

    // No polling for input state here!
    return Game::update(); 
}

void TicTacToe::_make_move()
{//places cursor down at current location, if possible
    if (_board[_cursor_index] == TicTacToePiece::EMPTY) {
        _board[_cursor_index] = _game_state==TicTacToePiece::PLAYER_CURSOR?TicTacToePiece::PLAYER:TicTacToePiece::MALO;
        _count_down[_cursor_index]=7;//init count-down timer

        while(_board[_cursor_index]!=TicTacToePiece::EMPTY) _cursor_index=(_cursor_index+1)%9; //find next cursor free space for next player to start at
        _malo_move=10;//clear malo move target

        bool is_win=_is_win();
        if(is_win)
        {
          if(_game_state==TicTacToePiece::PLAYER_CURSOR)
          {
            _game_state=TicTacToePiece::PLAYER_WINNER;
            if(!_is_player_win_seen){ _create_popup_overlay("You win!"); _is_player_win_seen=true; }
            _sensor_suite->save_state.unlock("Winner");
          } 
          else
          {
            _game_state=TicTacToePiece::MALO_WINNER;
            if(!_is_malo_win_seen){ _create_popup_overlay("MalO wins!"); _is_malo_win_seen=true; }
            else if(!_is_again_seen){ _create_popup_overlay("Again?"); _is_again_seen=true; }
            _sensor_suite->save_state.unlock("MalO Wins");
          }
          //TODO: game_save register achievement

        }else{
          for(uint8_t iter=0;iter<9;iter++)
          {
            //if(_count_down[iter]==1 && !_is_no_tie_seen){ _create_popup_overlay("No ties"); _is_no_tie_seen=true; }
            _count_down[iter]--;
            if(_count_down[iter]==0) _board[iter]=TicTacToePiece::EMPTY;
          }
          if(_game_state==TicTacToePiece::PLAYER_CURSOR) _game_state=TicTacToePiece::MALO_CURSOR;
          else _game_state=TicTacToePiece::PLAYER_CURSOR;
          if(_game_state==TicTacToePiece::MALO_CURSOR)
          {
            _malo_move=_get_malo_move(); //if malo move, then malo pick place to move to
            if(!_is_malo_turn_seen)
            {
                _create_popup_overlay("My turn");
                _is_malo_turn_seen=true;
                //while(1){ Serial.printf("HERE\n"); delay(100); }
            }
          }
        }
        
        // ⚡ CRITICAL: Force the screen to redraw the new piece instantly
        lv_obj_invalidate(_game_container);
        
        // Switch state to MalO's turn or check for win condition here
    }else{
        if(!_is_place_atop_seen){ _create_popup_overlay("Nice try"); _is_place_atop_seen=true; }
    }
}

// Static callback wrapper registered to your TicTacToe LVGL object
void TicTacToe::_game_key_cb(lv_event_t* e) {
    TicTacToe* instance = (TicTacToe*)lv_event_get_user_data(e);
    if (!instance) return;

    // Fetch the key pressed during this discrete event
    lv_key_t key = (lv_key_t)lv_event_get_key(e);

    // Process the input immediately when the key event fires
    
        // Anti-bounce: Only act if this is a new key press, not a continuous hold

    switch (key) {
        case LV_KEY_UP:
            if(instance->_game_state==TicTacToePiece::PLAYER_CURSOR) instance->_move_cursor(1); // Move Up
            break;
        case LV_KEY_DOWN:
            if(instance->_game_state==TicTacToePiece::PLAYER_CURSOR) instance->_move_cursor(2); // Move Down
            break;
        case LV_KEY_LEFT:
        case LV_KEY_PREV:
            if(instance->_game_state==TicTacToePiece::PLAYER_CURSOR) instance->_move_cursor(3); // Move Left
            break;
        case LV_KEY_RIGHT:
        case LV_KEY_NEXT:  // Handling your CW/CCW encoder mapping overrides
            if(instance->_game_state==TicTacToePiece::PLAYER_CURSOR) instance->_move_cursor(4); // Move Right
            break;
        case LV_KEY_ESC:
        case LV_KEY_ENTER:
            // Try to place a piece on the active square
            if(instance->_game_state==TicTacToePiece::PLAYER_CURSOR && key==LV_KEY_ENTER) instance->_make_move();//only valid to make move on player's turn
            else if(instance->_game_state==TicTacToePiece::PLAYER_WINNER || instance->_game_state==TicTacToePiece::MALO_WINNER) instance->_set_new_game();//clear board on move after game won
            break;
        //case LV_KEY_ESC: //confuses user if pressed accidentally
        //    instance->_set_new_game();
        //    break;
        case LV_KEY_HOME:
            // Correct scope to strongly-typed enum and fix the struct member name
            instance->_update_action.type = ScreenActionType::PUSH_SUBMENU;
            instance->_update_action.next_screen = instance->_screen_stack.empty() ? nullptr : instance->_screen_stack.front();

            break;
        //todo reset the game when there's a winner, or on enter key press too
            // Signal to the ScreenManager to pop back out to the menu hierarchy
            // no action on "no" 
            break;
        default:
            break;
    }

    // Force a redraw of the board now that the cursor or state has changed
    //lv_obj_invalidate(lv_event_get_target(e));
    lv_obj_invalidate((lv_obj_t*)lv_event_get_target(e));
}

void TicTacToe::_set_new_game()
{
    for(uint8_t iter=0;iter<9;iter++){ _board[iter]=TicTacToePiece::EMPTY; _count_down[iter]=0; }
    _game_state=_game_state==TicTacToePiece::PLAYER_WINNER?TicTacToePiece::MALO_CURSOR:TicTacToePiece::PLAYER_CURSOR;
    if(_game_state==TicTacToePiece::MALO_CURSOR) _malo_move=_get_malo_move(); //if malo move, then malo pick place to move to
    _cursor_index=0;

}

void TicTacToe::_move_cursor(uint8_t direction) {
    int row = _cursor_index / 3;
    int col = _cursor_index % 3;

    if (direction == 1) { // UP
        row = (row - 1 + 3) % 3;
    } 
    else if (direction == 2) { // DOWN
        row = (row + 1) % 3;
    } 
    else if (direction == 3) { // LEFT / CCW
        col = (col - 1 + 3) % 3;
    } 
    else if (direction == 4) { // RIGHT / CW
        col = (col + 1) % 3;
    }

    // Reconstruct the flat array index tracking point
    int new_index = row * 3 + col;
    
    if (_cursor_index != new_index) {
        _cursor_index = new_index;
        
        // ⚡ CRITICAL: Invalidate tells LVGL that the cursor moved, 
        // triggering your direct line-drawing callback to refresh the viewport!
        lv_obj_invalidate(_game_container); 
    }
}

void TicTacToe::end(bool is_leaving_upward)
{
  // Safely clean up active popups and timers immediately
  _clear_popup_overlay();

  if (is_leaving_upward)
  {
    if (_game_container != nullptr)
    {
      // Remove from the input group first if group exists
      if (_input_group)
      {
        lv_group_remove_obj(_game_container);
      }

      // Delete the container (automatically removes event callbacks and frees memory)
      lv_obj_delete(_game_container);
      _game_container = nullptr;
    }
  }
  else
  {
    lv_obj_add_flag(_game_container, LV_OBJ_FLAG_HIDDEN);
  }

  // FIX: Explicitly ensure this matches your updated Game interface
  Game::end(is_leaving_upward);
}

