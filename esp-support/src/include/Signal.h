#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

class Signal {
    SemaphoreHandle_t _sem;

public:
    Signal() : _sem(xSemaphoreCreateBinary()) {}
    ~Signal() { vSemaphoreDelete(_sem); }

    void signal() { xSemaphoreGive(_sem); }

    bool wait(TickType_t timeout = portMAX_DELAY) { return xSemaphoreTake(_sem, timeout) == pdTRUE; }
};