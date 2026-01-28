#include "support.h"

#include "esp_http_client.h"

LOG_TAG(support);

esp_err_t esp_http_download_string(const esp_http_client_config_t& config, std::string& target, size_t max_length,
                                   const char* authorization) {
    auto client = esp_http_client_init(&config);
    DEFER(esp_http_client_cleanup(client));

    if (authorization) {
        ESP_ERROR_RETURN(esp_http_client_set_header(client, "Authorization", authorization));
    }

    ESP_ERROR_RETURN(esp_http_client_open(client, 0));

    const auto length = esp_http_client_fetch_headers(client);
    if (length < 0) {
        ESP_ERROR_RETURN(-length);
    }

    return esp_http_get_response(client, target, max_length);
}

esp_err_t esp_http_upload_string(const esp_http_client_config_t& config, const char* const data,
                                 const char* content_type) {
    auto client = esp_http_client_init(&config);
    DEFER(esp_http_client_cleanup(client));

    ESP_ERROR_RETURN(esp_http_client_set_method(client, HTTP_METHOD_POST));
    ESP_ERROR_RETURN(esp_http_client_set_post_field(client, data, strlen(data)));

    if (content_type) {
        ESP_ERROR_RETURN(esp_http_client_set_header(client, "Content-Type", content_type));
    }

    return esp_http_client_perform(client);
}

int hextoi(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'A' && c <= 'F') {
        return 10 + (c - 'A');
    }
    if (c >= 'a' && c <= 'f') {
        return 10 + (c - 'a');
    }
    return -1;
}

char const* esp_reset_reason_to_name(esp_reset_reason_t reason) {
    switch (reason) {
        case ESP_RST_POWERON:
            return "ESP_RST_POWERON";
        case ESP_RST_EXT:
            return "ESP_RST_EXT";
        case ESP_RST_SW:
            return "ESP_RST_SW";
        case ESP_RST_PANIC:
            return "ESP_RST_PANIC";
        case ESP_RST_INT_WDT:
            return "ESP_RST_INT_WDT";
        case ESP_RST_TASK_WDT:
            return "ESP_RST_TASK_WDT";
        case ESP_RST_WDT:
            return "ESP_RST_WDT";
        case ESP_RST_DEEPSLEEP:
            return "ESP_RST_DEEPSLEEP";
        case ESP_RST_BROWNOUT:
            return "ESP_RST_BROWNOUT";
        case ESP_RST_SDIO:
            return "ESP_RST_SDIO";
        default:
            return "ESP_RST_UNKNOWN";
    }
}
