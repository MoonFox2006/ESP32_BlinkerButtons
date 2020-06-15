#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/portmacro.h>
#include <driver/gpio.h>

class Buttons {
public:
  enum btnstate_t { BTN_RELEASED, BTN_PRESSED, BTN_CLICK, BTN_DBLCLICK, BTN_LONGCLICK };
  struct __attribute__((__packed__)) btnevent_t {
    btnstate_t state : 3;
    uint8_t index : 5;
  };

  static const int8_t ERR_INDEX = -1;
  static const uint16_t DEF_QUEUE_DEPTH = 32;

  Buttons(uint8_t capacity, bool debounced = true, uint16_t queueDepth = DEF_QUEUE_DEPTH);
  ~Buttons();

  operator bool() {
    return _valid;
  }
  void clear();
  int8_t add(uint8_t pin, bool level = false, bool pullup = true);
  void remove(uint8_t index);
  uint8_t count() const {
    return _count;
  }
  int8_t find(uint8_t pin);

  QueueHandle_t eventQueue;

protected:
  struct __attribute__((__packed__)) button_t {
    uint8_t pin : 6;
    bool level : 1;
    volatile bool pressed : 1;
    volatile bool dbl : 1;
    volatile uint16_t duration : 15;
  };

  static const uint8_t MAX_CAPACITY = 32; // 2^5
  static const uint32_t CLICK_TIME = 20; // 20 ms. for debounce
  static const uint32_t DBLCLICK_TIME = 500; // 0.5 sec. maximum between two clicks
  static const uint32_t LONGCLICK_TIME = 1000; // 1 sec.

  void buttonISR();

  struct __attribute__((__packed__)) {
    gpio_isr_handle_t _gpioISR;
    portMUX_TYPE _mux;
    button_t *_items;
    bool _valid : 1;
    bool _debounced : 1;
    uint8_t _capacity : 6;
    uint8_t _count;
    volatile uint32_t _lastISR;
  };
};
