#pragma once

#include <vector>

#define BUZZER_PIN 25

class Buzzer {
private:
  // 1. Structure to hold queued tone payloads
  struct ToneFrame {
    float frequency_hz;
    float duration_ms;
    bool is_enabled;
  };

  std::vector<ToneFrame> _tone_queue; // The software queue
  
  PulseChain pulse_chain = PulseChain();
  bool _is_master_disable = false;
  uint8_t _pwm_pin = 0;
  uint64_t _last_update_us = 0; // when the note started to play
  uint64_t _duration_us = 0;    // how long to play note for
  bool _is_playing = false;     // Track if an active note is executing

public:
  Buzzer() {}
  
  void begin(PIOProgramManager &pio_program_manager, uint8_t pwm_pin = BUZZER_PIN) {
    _pwm_pin = pwm_pin;
    clear();
  }

  // 2. The Routine Update loop (called at 60 Hz)
  void update() { 
    // Force immediate mute if master disabled
    if (_is_master_disable) {
      clear();
      return;
    }

    // Check if the current note's elapsed time has exceeded its scheduled duration
    if (_is_playing && (time_us_64() - _last_update_us) >= _duration_us) {
      _is_playing = false;
      // Intentionally don't call clear() here to avoid brief 4kHz clicks between smooth sequential notes
    }

    // If no note is playing, try to process the next note in the vector queue
    if (!_is_playing && !_tone_queue.empty()) {
      // Pull the front item out of our FIFO sequence
      ToneFrame next_tone = _tone_queue.front();
      _tone_queue.erase(_tone_queue.begin()); // Remove from vector

      // Fire off the manual PWM configurations
      play_tone(next_tone.frequency_hz, next_tone.duration_ms, next_tone.is_enabled);
    } 
    // Queue is empty and nothing is running: park the hardware in a safe silent state
    else if (!_is_playing) {
      clear();
    }
  }

  // 3. Implemented: Append new notes dynamically to the vector sequence
  void append_tone(float frequency_hz, float duration_ms, bool is_enabled) {
    // If master disabled, we can silently ignore the append or push it as a muted frame
    if (_is_master_disable) is_enabled = false;

    ToneFrame frame = { frequency_hz, duration_ms, is_enabled };
    _tone_queue.push_back(frame);
  }

  // Overload helper for simple calls
  void append_tone(float frequency_hz, float duration_ms) {
    append_tone(frequency_hz, duration_ms, true);
  }

  // 4. Directly play single tone manually (bypasses queue / overrides instantly)
  void play_tone(float frequency_hz, float duration_ms, bool is_enabled) {
    if (_is_master_disable) is_enabled = false;

    gpio_set_function(_pwm_pin, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(_pwm_pin);
    uint channel = pwm_gpio_to_channel(_pwm_pin);
    pwm_set_wrap(slice_num, 255);
    
    // Set 50% duty cycle (127) if enabled, otherwise completely drop duty cycle to 0
    pwm_set_chan_level(slice_num, channel, is_enabled ? 127 : 0);
    
    float sys_clk_hz = (float)clock_get_hz(clk_sys);
    float dynamic_div = sys_clk_hz / (frequency_hz * 256.0f);
    pwm_set_clkdiv(slice_num, dynamic_div);
    pwm_set_enabled(slice_num, true); 
    
    _last_update_us = time_us_64();
    _duration_us = 1000ULL * (uint64_t)duration_ms;
    _is_playing = is_enabled; // Keep update state machine alive if it is an active sound
  }
  
  void play_tone(float frequency_hz, float duration_ms) { 
    play_tone(frequency_hz, duration_ms, true); 
  }

  // 5. Instantly purges the vector memory and silences the PWM slice
  void clear() { 
    _tone_queue.clear();
    _is_playing = false;
    _duration_us = 0;
    
    // Low overhead hardware absolute mute condition
    if (_pwm_pin != 0) {
      uint slice_num = pwm_gpio_to_slice_num(_pwm_pin);
      uint channel = pwm_gpio_to_channel(_pwm_pin);
      pwm_set_chan_level(slice_num, channel, 0); 
    }
  }

  // Retained for backwards compatibility interface matches
  void play() {}

  bool get_master_disable() { return _is_master_disable;}
  void set_master_disable(bool is_master_disable) {
    _is_master_disable = is_master_disable; 
    if (_is_master_disable) {
      clear();
    }
  }

  // Helpful utility to check if melody tracks are still executing
  bool is_busy() {
    return _is_playing || !_tone_queue.empty();
  }

  void end() { clear(); }
};