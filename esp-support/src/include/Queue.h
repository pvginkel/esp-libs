#pragma once

#include <functional>
#include <vector>

#include "freertos/FreeRTOS.h"

#ifdef LV_SIMULATOR
#include <deque>
#else
#include "freertos/portmacro.h"
#endif

class Queue {
#ifndef LV_SIMULATOR
    QueueHandle_t _queue;
    std::vector<std::pair<int64_t, std::function<void()>>> _delayed_tasks;
    portMUX_TYPE _delayed_tasks_lock = portMUX_INITIALIZER_UNLOCKED;
#else
    std::deque<function<void()>> _queue;
#endif

public:
    Queue();

    void enqueue(const std::function<void()>& task, bool wait = true);
#ifndef LV_SIMULATOR
    void enqueue_delayed(const std::function<void()>& task, uint32_t delay_ms);
#endif
    void process();

private:
    void handled_delayed_enqueues();
};
