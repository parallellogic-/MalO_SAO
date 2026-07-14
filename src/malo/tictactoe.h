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

enum class TicTacToeState{
  PLAYER_TURN,
  MALO_TURN,
  WON
};

class TicTacToe : public Game{
  private:
    TicTacToeState _game_state=TicTacToeState::PLAYER_TURN;//user takes first move, "X"
    TicTacToeState _board_state[9]={};
    uint8_t _count_down[9]={};//pieces placed will have a count-down timer, when zero, reverts to a free space (unable for players to tie)
  public:
    TicTacToe(const std::string& text, lv_group_t* shared_input_group);
    void begin(bool is_enter_from_above) override; //fetch resources from RAM like imagery or IR configuration 
    ScreenAction update() override; 
    void end(bool is_leaving_upward) override;
};