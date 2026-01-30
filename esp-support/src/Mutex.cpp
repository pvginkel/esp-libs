#include "Mutex.h"

#include "error.h"

MutexLock::MutexLock(Mutex* mutex) : _mutex(mutex) {}

MutexLock::~MutexLock() { _mutex->give(); }

Mutex::Mutex() {
    _lock = xSemaphoreCreateMutex();

    ESP_ASSERT_CHECK(_lock);
}

Mutex::~Mutex() { vSemaphoreDelete(_lock); }

MutexLock Mutex::take(TickType_t xTicksToWait) {
    ESP_ASSERT_CHECK(xSemaphoreTake(_lock, xTicksToWait));

    return {this};
}

void Mutex::give() { xSemaphoreGive(_lock); }
