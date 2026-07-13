#include "game.h"

Game::Game(const std::string& text, lv_group_t* shared_input_group) : Screen(text, shared_input_group) {}

void Game::begin(bool is_enter_from_above) 
{
    // Chain up to the base Screen class so it handles visibility/layouts
    Screen::begin(is_enter_from_above); 
}


ScreenAction Game::update() 
{
    // Chain up to the base Screen class so it handles visibility/layouts
    return _update_action;
}

void Game::end(bool is_leaving_upward) 
{
    // Chain up to the base Screen class to handle tear-down
    Screen::end(is_leaving_upward);
}