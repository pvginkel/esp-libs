#pragma once

#include "ha/esp_zigbee_ha_standard.h"

class ZigBeeLock {
public:
    explicit ZigBeeLock(TickType_t timeout = portMAX_DELAY) { esp_zb_lock_acquire(timeout); }

    ~ZigBeeLock() { esp_zb_lock_release(); }

    ZigBeeLock(const ZigBeeLock&) = delete;
    ZigBeeLock& operator=(const ZigBeeLock&) = delete;

    // (Optional) Delete move constructor and move assignment if moving doesn't make sense
    ZigBeeLock(ZigBeeLock&&) = delete;
    ZigBeeLock& operator=(ZigBeeLock&&) = delete;
};
