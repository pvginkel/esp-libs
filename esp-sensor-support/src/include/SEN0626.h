#pragma once

#include <cstdint>
#include <functional>

#include "Callback.h"
#include "esp_err.h"
#include "driver/i2c_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

class DFRobot_GestureFaceDetection_I2C;

struct GestureEvent {
    uint16_t type;
    uint16_t score;
};

class SEN0626 {
    Queue* _queue;
    DFRobot_GestureFaceDetection_I2C* _gfd{};
    i2c_master_bus_handle_t _bus_handle{};
    i2c_master_dev_handle_t _dev_handle{};
    TaskHandle_t _task_handle{};

    Callback<GestureEvent> _gesture_detected;

public:
    SEN0626(Queue* queue) : _queue(queue) {}

    esp_err_t begin(int sda_pin, int scl_pin, int int_pin);

    void on_gesture_detected(std::function<void(GestureEvent)> func) { _gesture_detected.add(func); }

private:
    static void IRAM_ATTR isr_handler(void* arg);
    void task_loop();
};
