#include "Queue.h"

#ifndef LV_SIMULATOR

#include <algorithm>

#include "esp_timer.h"

Queue::Queue() { _queue = xQueueCreate(32, sizeof(void*)); }

void Queue::enqueue(const std::function<void()>& task) {
    auto* copy = new std::function<void()>(task);
    xQueueSend(_queue, &copy, portMAX_DELAY);
}

void Queue::enqueue_delayed(const std::function<void()>& task, uint32_t delay_ms) {
    auto execute_at = esp_timer_get_time() + delay_ms * 1000;
    auto it = std::lower_bound(_delayed_tasks.begin(), _delayed_tasks.end(), execute_at,
                               [](const auto& entry, int64_t time) { return entry.first < time; });
    _delayed_tasks.insert(it, {execute_at, task});
}

void Queue::process() {
    handled_delayed_enqueues();

    while (uxQueueMessagesWaiting(_queue) > 0) {
        std::function<void()>* task;
        if (xQueueReceive(_queue, &task, 0) == pdTRUE) {
            (*task)();
            delete task;
        }
    }
}

void Queue::handled_delayed_enqueues() {
    auto now = esp_timer_get_time();
    while (!_delayed_tasks.empty() && _delayed_tasks.front().first <= now) {
        enqueue(_delayed_tasks.front().second);
        _delayed_tasks.erase(_delayed_tasks.begin());
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
