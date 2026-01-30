#pragma once

#include "error.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

class Signal {
    SemaphoreHandle_t _sem;

public:
    Signal() {
        _sem = xSemaphoreCreateBinary();
        ESP_ASSERT_CHECK(_sem);
    }

    ~Signal() { vSemaphoreDelete(_sem); }

    void signal() { xSemaphoreGive(_sem); }

    bool wait(TickType_t timeout = portMAX_DELAY) { return xSemaphoreTake(_sem, timeout) == pdTRUE; }
};