#pragma once

// Helpers for the component's ESP-IDF side. The pure core in core/ never includes this.

#include "error.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_timer.h"

#define LOG_TAG(v) [[maybe_unused]] static const char* TAG = #v
