#pragma once

class Buzzer {
private:
  uint64_t _last_update_us=0;
  bool _is_off=0;
  int32_t _timeout_ms=-1;

public:
    Buzzer(int32_t timeout_ms = 3000);
    
    void begin(uint8_t pin);

    bool update();//eval against timeout

    void set_tone(float frequency_hz,float duty_cycle_percent);
    void set_off();
};