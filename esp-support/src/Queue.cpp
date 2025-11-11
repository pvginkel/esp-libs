#include "Queue.h"

#ifndef LV_SIMULATOR

Queue::Queue() { _queue = xQueueCreate(32, sizeof(std::function<void()>)); }

void Queue::enqueue(const std::function<void()>& task) {
    auto* copy = new std::function<void()>(task);
    xQueueSend(_queue, &copy, portMAX_DELAY);
}

void Queue::process() {
    while (uxQueueMessagesWaiting(_queue) > 0) {
        std::function<void()>* task;
        if (xQueueReceive(_queue, &task, 0) == pdTRUE) {
            (*task)();
            delete task;
        }
    }
}

#else

Queue::Queue() {}

void Queue::enqueue(const std::function<void()>& task) { _queue.push_back(task); }

void Queue::process() {
    while (!_queue.empty()) {
        _queue.front()();
        _queue.pop_front();
    }
}

#endif
