#pragma once

#include <memory>
#include <string>

#include "defer.h"
#include "error.h"
#include "esp_check.h"
#include "esp_crt_bundle.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_system.h"
#include "strformat.h"

#define LOG_TAG(v) [[maybe_unused]] static const char* TAG = #v

#define ESP_TIMER_MS(v) ((v) * 1000)
#define ESP_TIMER_SECONDS(v) ESP_TIMER_MS((v) * 1000)

#define esp_get_millis() (esp_timer_get_time() / 1000ll)

int hextoi(char c);
char const* esp_reset_reason_to_name(esp_reset_reason_t reason);
