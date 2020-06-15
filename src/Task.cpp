#include <esp_log.h>
#include "Task.h"

static const char TAG[] = "Task";

Task::Task(const char *name, uint32_t stack, uint8_t priority, core_t core) {
  BaseType_t result;

  _flags = xEventGroupCreate();
  if (! _flags) {
    _task = NULL;
    ESP_LOGE(TAG, "Error creating RTOS event group!\r\n");
    return;
  }
  if (core < CORE_ANY)
    result = xTaskCreatePinnedToCore((TaskFunction_t)&Task::taskHandler, name, stack, this, priority, &_task, core);
  else
    result = xTaskCreate((TaskFunction_t)&Task::taskHandler, name, stack, this, priority, &_task);
  if (result != pdPASS) {
    _task = NULL;
    vEventGroupDelete(_flags);
    ESP_LOGE(TAG, "Error creating RTOS task!\r\n");
  }
}

bool Task::isRunning() {
  if (_task) {
    return eTaskGetState(_task) == eRunning;
  }
  return false;
}

void Task::pause() {
  if (_task) {
    vTaskSuspend(_task);
  }
}

void Task::resume() {
  if (_task) {
    vTaskResume(_task);
  }
}

void Task::destroy() {
  if (_task) {
    xEventGroupSetBits(_flags, FLAG_DESTROYING);
    xEventGroupWaitBits(_flags, FLAG_DESTROYED, pdFALSE, pdTRUE, portMAX_DELAY);
    vEventGroupDelete(_flags);
    _task = NULL;
  }
}

void Task::notify(uint32_t value) {
  if (_task) {
    xTaskNotify(_task, value, eSetValueWithOverwrite);
  }
}

void Task::lock() {
  portENTER_CRITICAL(&_mutex);
}

void Task::unlock() {
  portEXIT_CRITICAL(&_mutex);
}

void Task::taskHandler() {
  setup();
  while (! (xEventGroupWaitBits(_flags, FLAG_DESTROYING, pdFALSE, pdTRUE, 0) & FLAG_DESTROYING)) {
    loop();
  }
  cleanup();
  xEventGroupSetBits(_flags, FLAG_DESTROYED);
  vTaskDelete(_task);
}

portMUX_TYPE Task::_mutex = portMUX_INITIALIZER_UNLOCKED;
