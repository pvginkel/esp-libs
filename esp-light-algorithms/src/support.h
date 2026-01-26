#pragma once

#include <math.h>
#include <stdint.h>
#include <string.h>

#include "error.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "sdkconfig.h"

#define LOG_TAG(v) [[maybe_unused]] static const char *TAG = #v
