#include <Arduino.h>
#include "Blinker.h"
#include "Buttons.h"
#include "Task.h"

#define LED_PIN 22
#define LED_LEVEL LOW

class BlinkerTask : public Task {
public:
  BlinkerTask(uint8_t pin, bool level) : Task("BlinkerTask", 1024), _blinker(NULL), _pin(pin), _level(level) {}

protected:
  void setup();
  void loop();
  void cleanup();

  struct __attribute__((__packed__)) {
    Blinker *_blinker;
    uint8_t _pin : 7;
    bool _level : 1;
  };
};

void BlinkerTask::setup() {
  if (_task) {
    _blinker = new Blinker(_pin, _level);
    if ((! _blinker) || (! *_blinker)) {
      if (_blinker) {
        delete _blinker;
        _blinker= NULL;
      }
      destroy();
    }
  }
}

void BlinkerTask::loop() {
  const char *BLINKS[] = { "OFF", "ON", "TOGGLE", "0.5 Hz", "1 Hz", "2 Hz", "4 Hz", "FADE IN", "FADE OUT", "FADE IN/OUT" };

  Blinker::blinkmode_t mode = _blinker->getMode();

  if (mode < Blinker::BLINK_FADEINOUT)
    mode = (Blinker::blinkmode_t)((uint8_t)mode + 1);
  else
    mode = Blinker::BLINK_OFF;
  *_blinker = mode;
  lock();
  Serial.print("Blinker switch to ");
  Serial.println(BLINKS[mode]);
  unlock();
  vTaskDelay(pdMS_TO_TICKS(5000));
}

void BlinkerTask::cleanup() {
  if (_blinker) {
    delete _blinker;
    _blinker = NULL;
  }
}

Buttons *buttons = NULL;
Task *task = NULL;

static void halt(const char *msg) {
  if (buttons)
    delete buttons;
  if (task)
    delete task;
  Serial.println(msg);
  Serial.flush();
  esp_deep_sleep_start();
}

void setup() {
  Serial.begin(115200);
  Serial.println();

  task = new BlinkerTask(LED_PIN, LED_LEVEL);
  if ((! task) || (! *task))
    halt("Error initializing blinker task!");
  buttons = new Buttons(4, false);
  if ((! buttons) || (! *buttons))
    halt("Error initializing buttons!");
  buttons->add(26);
  buttons->add(27);
  buttons->add(12);
  buttons->add(14);
}

void loop() {
  Buttons::btnevent_t btnevent;

  if (xQueueReceive(buttons->eventQueue, &btnevent, portMAX_DELAY) == pdTRUE) {
    const char *STATES[] = { "released", "pressed", "clicked", "double clicked", "long clicked" };

    Task::lock();
    Serial.print("Button #");
    Serial.print(btnevent.index);
    Serial.print(' ');
    Serial.println(STATES[btnevent.state]);
    Task::unlock();
  }
}
