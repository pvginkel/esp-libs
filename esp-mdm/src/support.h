#pragma once

#include "error.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_http_client.h"
#include "esp_log.h"

#define LOG_TAG(v) [[maybe_unused]] static const char* TAG = #v

#define ESP_TIMER_MS(v) ((v) * 1000)
#define ESP_TIMER_SECONDS(v) ESP_TIMER_MS((v) * 1000)

#define esp_get_millis() uint32_t(esp_timer_get_time() / 1000ull)

esp_err_t esp_http_upload_string(const esp_http_client_config_t& config, const char* const data);
int hextoi(char c);
char const* esp_reset_reason_to_name(esp_reset_reason_t reason);
