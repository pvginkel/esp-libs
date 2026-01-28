#pragma once

#include <memory>
#include <string>

#include "defer.h"
#include "error.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "strformat.h"

#define LOG_TAG(v) [[maybe_unused]] static const char* TAG = #v

#define ESP_TIMER_MS(v) ((v) * 1000)
#define ESP_TIMER_SECONDS(v) ESP_TIMER_MS((v) * 1000)

#define esp_get_millis() uint32_t(esp_timer_get_time() / 1000ull)

esp_err_t esp_http_get_response(esp_http_client_handle_t client, std::string& target, size_t max_length);
esp_err_t esp_http_download_string(const esp_http_client_config_t& config, std::string& target, size_t max_length = 0,
                                   const char* authorization = nullptr);
esp_err_t esp_http_upload_string(const esp_http_client_config_t& config, const char* const data,
                                 const char* content_type = nullptr);
int hextoi(char c);
char const* esp_reset_reason_to_name(esp_reset_reason_t reason);
