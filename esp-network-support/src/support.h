#pragma once

#include "error.h"

#define LOG_TAG(v) [[maybe_unused]] static const char* TAG = #v

#define esp_get_millis() uint32_t(esp_timer_get_time() / 1000ull)
