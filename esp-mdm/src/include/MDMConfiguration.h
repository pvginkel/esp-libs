#pragma once

#include <string>

#include "esp_err.h"

class MDMConfiguration {
    std::string _device_key;
    std::string _client_id;
    std::string _client_secret;
    std::string _token_url;
    std::string _base_url;
    std::string _mqtt_url;
    std::string _wifi_ssid;
    std::string _wifi_password;

public:
    esp_err_t load();

    const std::string& get_device_key() const { return _device_key; }
    const std::string& get_client_id() const { return _client_id; }
    const std::string& get_client_secret() const { return _client_secret; }
    const std::string& get_token_url() const { return _token_url; }
    const std::string& get_base_url() const { return _base_url; }
    const std::string& get_mqtt_url() const { return _mqtt_url; }
    const std::string& get_wifi_ssid() const { return _wifi_ssid; }
    const std::string& get_wifi_password() const { return _wifi_password; }
};