#pragma once

#include <algorithm>
#include <memory>

#include "error.h"
#include "esp_check.h"

#define LOG_TAG(v) [[maybe_unused]] static const char* TAG = #v
