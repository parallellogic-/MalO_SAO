#include "tictactoe.h"

TicTacToe::TicTacToe(const std::string& text, lv_group_t* shared_input_group):Game(text,shared_input_group){}

void TicTacToe::begin(bool is_enter_from_above)
{
  Game::begin(is_enter_from_above);
  
  _game_container = lv_obj_create(_lv_panel);
  lv_obj_set_size(_game_container, SCREEN_WIDTH_PX, SCREEN_HEIGHT_PX - HEADER_HEIGHT_PX);
  
  // Use explicit top-left alignment with Y offset to sit perfectly below the header!
  lv_obj_align(_game_container, LV_ALIGN_TOP_LEFT, 0, HEADER_HEIGHT_PX);
  
  lv_obj_set_style_bg_color(_game_container, lv_color_black(), 0);
  lv_obj_set_style_border_width(_game_container, 0, 0);
  lv_obj_set_style_pad_all(_game_container, 0, 0);
  lv_obj_remove_flag(_game_container, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_add_event_cb(_game_container, _game_draw_cb, LV_EVENT_DRAW_MAIN, this);
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

    // --- STEP 1: Draw Grid Lines (Corrected for LVGL v9) ---
    lv_draw_line_dsc_t line_dsc;
    lv_draw_line_dsc_init(&line_dsc);
    line_dsc.color = lv_color_hex(0x404040); // Dim grey lines
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

        lv_area_t cell_area;
        cell_area.x1 = board_coords.x1 + (col * cell_w) + 4;
        cell_area.x2 = cell_area.x1 + cell_w - 8;
        cell_area.y1 = board_coords.y1 + (row * cell_h) + 4;
        cell_area.y2 = cell_area.y1 + cell_h - 8;

        // Draw selection cursor highlight box
        if (i == instance->_cursor_index) {
            rect_dsc.bg_opa = LV_OPA_TRANSP;
            rect_dsc.border_color = lv_color_white();
            rect_dsc.border_width = 1;
            rect_dsc.radius = 0; // Turn off rounding processing loops
            lv_draw_rect(layer, &rect_dsc, &cell_area);
        }

        // Draw Content symbols
        if (instance->_board[i] == TicTacToePiece::PLAYER) {
            rect_dsc.bg_color = lv_color_hex(0xFF0000); // Red box for X
            rect_dsc.bg_opa = LV_OPA_COVER;
            rect_dsc.border_width = 0;
            rect_dsc.radius = 0;
            lv_draw_rect(layer, &rect_dsc, &cell_area);
        } 
        else if (instance->_board[i] == TicTacToePiece::MALO) {
            rect_dsc.bg_color = lv_color_hex(0x0000FF); // Blue box for O
            rect_dsc.bg_opa = LV_OPA_COVER;
            rect_dsc.border_width = 0;
            rect_dsc.radius = 0;
            lv_draw_rect(layer, &rect_dsc, &cell_area);
        }
    }
}


ScreenAction TicTacToe::update()
{
  

  return _update_action;
}

void TicTacToe::end(bool is_leaving_upward)
{
  

  Game::end(is_leaving_upward);
}