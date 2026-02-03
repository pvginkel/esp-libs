#include "support.h"

#include "MDMConfiguration.h"

#include <nvs.h>

#include <set>

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

    ESP_LOGI(TAG, "Configuration loaded:");
    ESP_LOGI(TAG, "  device_key: %s", _device_key.c_str());
    ESP_LOGI(TAG, "  client_id: %s", _client_id.c_str());
    ESP_LOGI(TAG, "  client_secret: ***");
    ESP_LOGI(TAG, "  token_url: %s", _token_url.c_str());
    ESP_LOGI(TAG, "  base_url: %s", _base_url.c_str());
    ESP_LOGI(TAG, "  mqtt_url: %s", _mqtt_url.c_str());
    ESP_LOGI(TAG, "  wifi_ssid: %s", _wifi_ssid.c_str());
    ESP_LOGI(TAG, "  wifi_password: %s", _wifi_password.c_str());

    if (!_base_url.ends_with('/')) {
        _base_url += '/';
    }

    return ESP_OK;
}

esp_err_t MDMConfiguration::save_provisioning(cJSON* data) {
    // Validate that all items are strings and collect keys.
    std::set<std::string> new_keys;
    for (auto item = data->child; item != nullptr; item = item->next) {
        if (!cJSON_IsString(item) || !item->valuestring) {
            ESP_LOGE(TAG, "Provisioning data contains non-string value for key '%s'", item->string);
            return ESP_ERR_INVALID_ARG;
        }
        new_keys.insert(item->string);
    }

    nvs_handle_t handle;
    ESP_ERROR_RETURN(nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle));
    DEFER(nvs_close(handle));

    // Delete stale keys that are in NVS but not in the new provisioning data.
    nvs_iterator_t it = nullptr;
    auto err = nvs_entry_find(NVS_DEFAULT_PART_NAME, NVS_NAMESPACE, NVS_TYPE_STR, &it);
    while (err == ESP_OK) {
        nvs_entry_info_t info;
        nvs_entry_info(it, &info);

        if (new_keys.find(info.key) == new_keys.end()) {
            ESP_LOGI(TAG, "Deleting stale provisioning key '%s'", info.key);
            ESP_ERROR_RETURN(nvs_erase_key(handle, info.key));
        }

        err = nvs_entry_next(&it);
    }
    nvs_release_iterator(it);

    // Write new keys.
    for (auto item = data->child; item != nullptr; item = item->next) {
        ESP_LOGI(TAG, "Writing provisioning key '%s'", item->string);
        ESP_ERROR_RETURN(nvs_set_str(handle, item->string, item->valuestring));
    }

    ESP_ERROR_RETURN(nvs_commit(handle));

    ESP_LOGI(TAG, "Provisioning data saved successfully");

    return ESP_OK;
}
