#pragma once

#include <functional>

#include "freertos/FreeRTOS.h"

#ifdef LV_SIMULATOR
#include <deque>
#endif

class Queue {
#ifndef LV_SIMULATOR
    QueueHandle_t _queue;
#else
    std::deque<function<void()> > _queue;
#endif

public:
    Queue();

    void enqueue(const std::function<void()> &task);
    void process();
};