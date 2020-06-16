#include <string.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <esp_intr_alloc.h>
#include "Buttons.h"

static const char TAG[] = "Buttons";

Buttons::Buttons(uint8_t capacity, bool debounced, uint16_t queueDepth) {
  _valid = false;
  if (capacity > MAX_CAPACITY) {
    ESP_LOGE(TAG, "Too many buttons!\r\n");
    return;
  }
  _debounced = debounced;
  _capacity = capacity;
  _count = 0;
  _lastISR = 0;
  _mux = portMUX_INITIALIZER_UNLOCKED;
  eventQueue = xQueueCreate(queueDepth, sizeof(btnevent_t));
  if (! eventQueue) {
    ESP_LOGE(TAG, "Error allocating event queue!\r\n");
    return;
  }
  _items = new button_t[capacity];
  if (! _items) {
    vQueueDelete(eventQueue);
    eventQueue = NULL;
    ESP_LOGE(TAG, "Error allocating button list!\r\n");
    return;
  }
  if (gpio_isr_register((gpio_isr_t)&Buttons::buttonISR, this, ESP_INTR_FLAG_IRAM, &_gpioISR) != ESP_OK) {
    delete[] _items;
    _items = NULL;
    vQueueDelete(eventQueue);
    eventQueue = NULL;
    ESP_LOGE(TAG, "Error registering GPIO interrupt!\r\n");
    return;
  }
  _valid = true;
}

Buttons::~Buttons() {
  if (_valid) {
    esp_intr_free(_gpioISR);
    if (_items) {
      clear();
      delete[] _items;
    }
    if (eventQueue)
      vQueueDelete(eventQueue);
  }
}

void Buttons::clear() {
  if (_valid) {
    for (int8_t i = _count - 1; i >= 0; --i) {
      remove(i);
    }
  }
}

int8_t Buttons::add(uint8_t pin, bool level, bool pullup) {
  int8_t result = ERR_INDEX;

  if (_valid) {
    int8_t result = find(pin);

    portENTER_CRITICAL(&_mux);
    if ((result == ERR_INDEX) && (_count < _capacity)) {
      result = _count++;
    }
    if (result != ERR_INDEX) {
      _items[result].pin = pin;
      _items[result].level = level;
      _items[result].pressed = false;
      _items[result].dbl = false;
      _items[result].duration = 0;
      portEXIT_CRITICAL(&_mux);
      gpio_set_direction((gpio_num_t)pin, GPIO_MODE_INPUT);
      gpio_set_pull_mode((gpio_num_t)pin, pullup ? GPIO_PULLUP_ONLY : GPIO_FLOATING);
      gpio_set_intr_type((gpio_num_t)pin, GPIO_INTR_ANYEDGE);
      gpio_intr_enable((gpio_num_t)pin);
      if (gpio_get_level((gpio_num_t)pin) == level) { // Button already pressed
        uint32_t time = esp_timer_get_time() / 1000;

        portENTER_CRITICAL(&_mux);
        if (_lastISR) {
          uint32_t t = time - _lastISR;

          for (uint8_t i = 0; i < result; ++i) {
            if (_items[i].pressed) { // Button was pressed
              if (t + _items[i].duration >= 0x7FFF)
                _items[i].duration = 0x7FFF;
              else
                _items[i].duration += t;
            } else { // Button was released
              if (_items[i].dbl) {
                if (t >= _items[i].duration) {
                  _items[i].dbl = false;
                  _items[i].duration = 0;
                } else {
                  _items[i].duration -= t;
                }
              }
            }
          }
        }
        _items[result].pressed = true;
        _lastISR = time;
        portEXIT_CRITICAL(&_mux);
        if (! _debounced) {
          btnevent_t btnevent = {
            .state = BTN_PRESSED,
            .index = result
          };

          xQueueSend(eventQueue, &btnevent, 0);
        }
      }
    } else
      portEXIT_CRITICAL(&_mux);
  }
  return result;
}

void Buttons::remove(uint8_t index) {
  if (_valid) {
    portENTER_CRITICAL(&_mux);
    if (index < _count) {
      gpio_set_intr_type((gpio_num_t)_items[index].pin, GPIO_INTR_DISABLE);
      gpio_intr_disable((gpio_num_t)_items[index].pin);
      gpio_reset_pin((gpio_num_t)_items[index].pin);
      if (index < _count - 1) {
        memcpy(&_items[index], &_items[index + 1], sizeof(button_t) * (_count - index - 1));
      }
      --_count;
    }
    portEXIT_CRITICAL(&_mux);
  }
}

int8_t Buttons::find(uint8_t pin) {
  int8_t result = ERR_INDEX;

  if (_valid) {
    portENTER_CRITICAL(&_mux);
    for (int8_t i = 0; i < _count; ++i) {
      if (_items[i].pin == pin) {
        result = i;
        break;
      }
    }
    portEXIT_CRITICAL(&_mux);
  }
  return result;
}

void IRAM_ATTR Buttons::buttonISR() {
  SET_PERI_REG_MASK(GPIO_STATUS_W1TC_REG, READ_PERI_REG(GPIO_STATUS_REG)); // Clear interrupt status for GPIO0-31
  SET_PERI_REG_MASK(GPIO_STATUS1_W1TC_REG, READ_PERI_REG(GPIO_STATUS1_REG)); // Clear interrupt status for GPIO32-39
  if (_valid) {
    portENTER_CRITICAL_ISR(&_mux);
    if (_count) {
      uint32_t time = esp_timer_get_time() / 1000;
      uint32_t gpi0 = READ_PERI_REG(GPIO_IN_REG);
      uint32_t gpi1 = READ_PERI_REG(GPIO_IN1_REG);
      uint32_t t;

      if (_lastISR)
        t = time - _lastISR;
      else
        t = 0;
      for (int8_t i = 0; i < _count; ++i) {
        bool pressed;
        btnevent_t btnevent;

        if (t) {
          if (_items[i].pressed) { // Button was pressed
            if (t + _items[i].duration >= 0x7FFF)
              _items[i].duration = 0x7FFF;
            else
              _items[i].duration += t;
          } else { // Button was released
            if (_items[i].dbl) {
              if (t >= _items[i].duration) {
                _items[i].dbl = false;
                _items[i].duration = 0;
              } else {
                _items[i].duration -= t;
              }
            }
          }
        }
        if (_items[i].pin < 32)
          pressed = ((gpi0 >> _items[i].pin) & 0x01) == _items[i].level;
        else
          pressed = ((gpi1 >> (_items[i].pin - 32)) & 0x01) == _items[i].level;
        if (pressed) { // Button pressed
          if (! _items[i].pressed) {
            if (! _debounced) {
              btnevent.state = BTN_PRESSED;
              btnevent.index = i;
              xQueueSendFromISR(eventQueue, &btnevent, NULL);
            }
            _items[i].pressed = true;
            _items[i].duration = 0;
          }
        } else { // Button released
          if (_items[i].pressed) {
            if (_items[i].duration >= LONGCLICK_TIME) {
              btnevent.state = BTN_LONGCLICK;
            } else if (_items[i].duration >= CLICK_TIME) {
              if (_items[i].dbl)
                btnevent.state = BTN_DBLCLICK;
              else
                btnevent.state = BTN_CLICK;
            } else {
              btnevent.state = BTN_RELEASED;
            }
            if ((btnevent.state != BTN_RELEASED) || (! _debounced)) {
              btnevent.index = i;
              xQueueSendFromISR(eventQueue, &btnevent, NULL);
            }
            _items[i].pressed = false;
            _items[i].dbl = true;
            _items[i].duration = DBLCLICK_TIME;
          }
        }
      }
      _lastISR = time;
    }
    portEXIT_CRITICAL_ISR(&_mux);
  }
}
