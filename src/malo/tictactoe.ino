#include "tictactoe.h"

TicTacToe::TicTacToe(const std::string& text, lv_group_t* shared_input_group):Game(text,shared_input_group){}

void TicTacToe::begin(bool is_enter_from_above)
{
  Game::begin(is_enter_from_above);


}

ScreenAction TicTacToe::update()
{
  

  return _update_action;
}

void TicTacToe::end(bool is_leaving_upward)
{
  

  Game::end(is_leaving_upward);
}