#include "support.h"

#include "MDMConfiguration.h"

#include <nvs.h>

#include "Defer.h"

#define NVS_NAMESPACE "prov"

static esp_err_t nvs_get_string(nvs_handle_t handle, const char* key, std::string& value) {
    size_t length;
    auto err = nvs_get_str(handle, key, nullptr, &length);
    if (err != ESP_OK) {
        return err;
    }

    value.resize(length - 1);
    return nvs_get_str(handle, key, value.data(), &length);
}

LOG_TAG(MDMConfiguration);

esp_err_t MDMConfiguration::load() {
    nvs_handle_t handle;

    auto err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        return err;
    }

    DEFER(nvs_close(handle));

    err = nvs_get_string(handle, "device_key", _device_key);
    if (err != ESP_OK) {
        return err;
    }
    err = nvs_get_string(handle, "client_id", _client_id);
    if (err != ESP_OK) {
        return err;
    }
    err = nvs_get_string(handle, "client_secret", _client_secret);
    if (err != ESP_OK) {
        return err;
    }
    err = nvs_get_string(handle, "token_url", _token_url);
    if (err != ESP_OK) {
        return err;
    }
    err = nvs_get_string(handle, "base_url", _base_url);
    if (err != ESP_OK) {
        return err;
    }
    err = nvs_get_string(handle, "mqtt_url", _mqtt_url);
    if (err != ESP_OK) {
        return err;
    }
    err = nvs_get_string(handle, "wifi_ssid", _wifi_ssid);
    if (err != ESP_OK) {
        return err;
    }
    err = nvs_get_string(handle, "wifi_password", _wifi_password);
    if (err != ESP_OK) {
        return err;
    }

    return ESP_OK;
}
