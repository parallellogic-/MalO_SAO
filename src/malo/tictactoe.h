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
    
    uint32_t _frame_id=0;
    uint8_t _malo_move=10;
    bool _is_malo_turn_seen=false;
    bool _is_player_win_seen=false;
    bool _is_malo_win_seen=false;
    bool _is_again_seen=false;
    bool _is_no_tie_seen=false;
    bool _is_place_atop_seen=false;

    static void _game_draw_cb(lv_event_t* e);
    static void _game_key_cb(lv_event_t* e);
    void _move_cursor(uint8_t direction); // Logic to map keys to grid navigation
    void _draw_piece(lv_layer_t* layer, lv_area_t& board_coords, TicTacToePiece piece,uint32_t frame_id,bool is_imminent_delete);
    bool _is_win();
    uint8_t _get_malo_move();
    void _make_move();
    void _set_new_game();
  protected:

  public:
    TicTacToe(const std::string& text, lv_group_t* shared_input_group);
    void begin(bool is_enter_from_above,SensorSuite *sensor_suite) override; //fetch resources from RAM like imagery or IR configuration 
    ScreenAction update() override; 
    void end(bool is_leaving_upward) override;

    lv_key_t touch_to_key(uint8_t touch) override{
        static const lv_key_t touch2key[] = {(lv_key_t)0,(lv_key_t)0,LV_KEY_HOME,LV_KEY_ESC,LV_KEY_ENTER,LV_KEY_LEFT,LV_KEY_UP,LV_KEY_RIGHT,LV_KEY_LEFT,LV_KEY_DOWN,LV_KEY_RIGHT};
        //unused, hidden, menu, no, yes, CCW, up, CW, left, down, right
        if(touch>=sizeof(touch2key)/sizeof(touch2key[0])) return (lv_key_t)0; return touch2key[touch]; }
};