#include "support.h"

#include "MDMConfiguration.h"

#include <nvs.h>

#include "defer.h"

#define NVS_NAMESPACE "prov"

LOG_TAG(MDMConfiguration);

static esp_err_t nvs_get_string(nvs_handle_t handle, const char* key, std::string& value) {
    size_t length;
    ESP_ERROR_RETURN(nvs_get_str(handle, key, nullptr, &length));

    value.resize(length - 1);
    return nvs_get_str(handle, key, value.data(), &length);
}

esp_err_t MDMConfiguration::load() {
    nvs_handle_t handle;

    ESP_ERROR_RETURN(nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle));

    DEFER(nvs_close(handle));

    ESP_ERROR_RETURN(nvs_get_string(handle, "device_key", _device_key));
    ESP_ERROR_RETURN(nvs_get_string(handle, "client_id", _client_id));
    ESP_ERROR_RETURN(nvs_get_string(handle, "client_secret", _client_secret));
    ESP_ERROR_RETURN(nvs_get_string(handle, "token_url", _token_url));
    ESP_ERROR_RETURN(nvs_get_string(handle, "base_url", _base_url));
    ESP_ERROR_RETURN(nvs_get_string(handle, "mqtt_url", _mqtt_url));
    ESP_ERROR_RETURN(nvs_get_string(handle, "wifi_ssid", _wifi_ssid));
    ESP_ERROR_RETURN(nvs_get_string(handle, "wifi_password", _wifi_password));
    ESP_ERROR_RETURN(nvs_get_string(handle, "logging_url", _logging_url));

    if (!_base_url.ends_with('/')) {
        _base_url += '/';
    }

    return ESP_OK;
}
