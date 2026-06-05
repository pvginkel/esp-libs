#include "Queue.h"

#include "error.h"

#ifndef LV_SIMULATOR

#include <algorithm>

#include "esp_timer.h"

Queue::Queue() {
    // 50 slots: headroom for bursts of event-task enqueues (e.g. a flood of
    // discovery prunes) before producers feel back-pressure.
    _queue = xQueueCreate(50, sizeof(void*));

    ESP_ASSERT_CHECK(_queue);
}

void Queue::enqueue(const std::function<void()>& task, bool wait) {
    auto* copy = new std::function<void()>(task);

    ESP_ASSERT_CHECK(xQueueSend(_queue, &copy, wait ? portMAX_DELAY : 0));
}

bool Queue::try_enqueue(const std::function<void()>& task) {
    auto* copy = new std::function<void()>(task);

    // Never wait for room. Unlike enqueue(), a full queue is an expected outcome,
    // not an assertion failure: free the copy and report the drop to the caller.
    if (xQueueSend(_queue, &copy, 0) != pdTRUE) {
        delete copy;
        return false;
    }

    return true;
}

void Queue::enqueue_delayed(const std::function<void()>& task, uint32_t delay_ms) {
    auto execute_at = esp_timer_get_time() + delay_ms * 1000;

    portENTER_CRITICAL(&_delayed_tasks_lock);

    auto it = std::lower_bound(_delayed_tasks.begin(), _delayed_tasks.end(), execute_at,
                               [](const auto& entry, int64_t time) { return entry.first < time; });
    _delayed_tasks.insert(it, {execute_at, task});

    portEXIT_CRITICAL(&_delayed_tasks_lock);
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

    portENTER_CRITICAL(&_delayed_tasks_lock);

    while (!_delayed_tasks.empty() && _delayed_tasks.front().first <= now) {
        auto task = _delayed_tasks.front().second;
        _delayed_tasks.erase(_delayed_tasks.begin());

        enqueue(task, false /* wait */);
    }

    portEXIT_CRITICAL(&_delayed_tasks_lock);
}

#else

Queue::Queue() {}

void Queue::enqueue(const std::function<void()>& task, bool wait) { _queue.push_back(task); }

bool Queue::try_enqueue(const std::function<void()>& task) {
    _queue.push_back(task);
    return true;
}

void Queue::process() {
    while (!_queue.empty()) {
        _queue.front()();
        _queue.pop_front();
    }
}

#endif
