#pragma once

#include <esp_timer.h>
#include <driver/ledc.h>

class Blinker {
public:
  enum blinkmode_t { BLINK_OFF, BLINK_ON, BLINK_TOGGLE, BLINK_05HZ, BLINK_1HZ, BLINK_2HZ, BLINK_4HZ, BLINK_FADEIN, BLINK_FADEOUT, BLINK_FADEINOUT };

  Blinker(uint8_t pin, bool level, uint32_t freq = 1000, ledc_timer_t timer_num = LEDC_TIMER_3, ledc_mode_t speed_mode = LEDC_LOW_SPEED_MODE, ledc_channel_t channel = LEDC_CHANNEL_7);
  ~Blinker();

  operator bool() {
    return _timer != NULL;
  }
  void operator=(blinkmode_t value) {
    setMode(value);
  }
  blinkmode_t getMode() const {
    return _mode;
  }
  void setMode(blinkmode_t mode);

protected:
  void timerCallback();

  struct __attribute__((__packed__)) {
    uint8_t _pin : 7;
    bool _level : 1;
    blinkmode_t _mode : 4;
    ledc_timer_t _timer_num : 3;
    ledc_mode_t _speed_mode : 2;
    ledc_channel_t _channel : 4;
    volatile int8_t _value;
    esp_timer_handle_t _timer;
  };
};
