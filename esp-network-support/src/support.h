#pragma once

#include "esp_http_client.h"

#define LOG_TAG(v) [[maybe_unused]] static const char* TAG = #v

#define ESP_TIMER_MS(v) ((v) * 1000)
#define ESP_TIMER_SECONDS(v) ESP_TIMER_MS((v) * 1000)

#define ESP_ERROR_CHECK_JUMP(x, label)                                     \
    do {                                                                   \
        esp_err_t err_rc_ = (x);                                           \
        if (unlikely(err_rc_ != ESP_OK)) {                                 \
            ESP_LOGE(TAG, #x " failed with %s", esp_err_to_name(err_rc_)); \
            goto label;                                                    \
        }                                                                  \
    } while (0)

#define esp_get_millis() uint32_t(esp_timer_get_time() / 1000ull)

esp_err_t esp_http_upload_string(const esp_http_client_config_t& config, const char* const data);
int hextoi(char c);
