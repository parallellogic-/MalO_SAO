#pragma once

#include "game.h"

#define TICTACTOE_BLINK_PERIOD 30 //how many frames for one blink cycle

enum class TicTacToePiece : uint8_t {
  EMPTY,
  PLAYER_CURSOR,
  MALO_CURSOR,
  PLAYER,
  MALO,
  PLAYER_WINNER, //highlight winner
  MALO_WINNER
};


class TicTacToe : public Game{
  private:
    TicTacToePiece _game_state=TicTacToePiece::PLAYER_CURSOR;//user takes first move, "X"
    int _cursor_index = 0; // Current active square (0 to 8)
    TicTacToePiece _board[9]={};
    uint8_t _count_down[9]={};//pieces placed will have a count-down timer, when zero, reverts to a free space (unable for players to tie)
    lv_obj_t* _game_container;
    uint32_t _frame_id=0;

    static void _game_draw_cb(lv_event_t* e);
    static void _game_key_cb(lv_event_t* e);
    void _move_cursor(uint8_t direction); // Logic to map keys to grid navigation
    void _draw_piece(lv_layer_t* layer, lv_area_t& board_coords, TicTacToePiece piece,uint32_t frame_id);
    bool _is_win();
  public:
    TicTacToe(const std::string& text, lv_group_t* shared_input_group);
    void begin(bool is_enter_from_above) override; //fetch resources from RAM like imagery or IR configuration 
    ScreenAction update() override; 
    void end(bool is_leaving_upward) override;
};