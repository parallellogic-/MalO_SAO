#pragma once

#include "game.h"

enum class TicTacToePiece{
  NONE,
  PLAYER_PRE, //pre-placement
  MALO_PRE,
  PLAYER,
  MALO,
  PLAYER_WINNER, //highlight winner
  MALO_WINNER
};

class TicTacToe : public Game{
  private:
    TicTacToePiece _game_state=TicTacToePiece::PLAYER_PRE;//user takes first move, "X"
    int _cursor_index = 0; // Current active square (0 to 8)
    TicTacToePiece _board[9]={};
    uint8_t _count_down[9]={};//pieces placed will have a count-down timer, when zero, reverts to a free space (unable for players to tie)
    lv_obj_t* _game_container;

    static void _game_draw_cb(lv_event_t* e);
    void _move_cursor(uint8_t direction); // Logic to map keys to grid navigation
  public:
    TicTacToe(const std::string& text, lv_group_t* shared_input_group);
    void begin(bool is_enter_from_above) override; //fetch resources from RAM like imagery or IR configuration 
    ScreenAction update() override; 
    void end(bool is_leaving_upward) override;
};