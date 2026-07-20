#pragma once

#define MOTOR_PIN 39

class Motor {
private:
  uint64_t _last_update_us=0;
  int32_t _timeout_ms=-1;
  bool _is_master_disable=false;

public:
    Motor(int32_t timeout_ms = 60):_timeout_ms(timeout_ms){};
    
    void begin(){pinMode(MOTOR_PIN,OUTPUT); set_off(); }
    void end(){pinMode(MOTOR_PIN,INPUT);}

    void set_off(){ digitalWrite(MOTOR_PIN,0); }
    void set_on(){ if(_is_master_disable) return; _last_update_us=time_us_64(); digitalWrite(MOTOR_PIN,1); }
    void update(){ if(_timeout_ms>=0 && (time_us_64()-_last_update_us)/1000>_timeout_ms) set_off(); }//eval against timeout
    void set_master_disable(bool is_master_disable){_is_master_disable=is_master_disable;}
};