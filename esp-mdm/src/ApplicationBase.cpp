#include "support.h"

#include "ApplicationBase.h"

#include "driver/i2c.h"
#include "nvs_flash.h"

LOG_TAG(ApplicationBase);

ApplicationBase::ApplicationBase() : _network_connection(&_queue), _mqtt_connection(&_queue) {}

void ApplicationBase::begin(bool silent) {
    ESP_LOGI(TAG, "Setting up the log manager");

    ESP_ERROR_CHECK(_mdm_configuration.load());

    ESP_ERROR_CHECK(_log_manager.begin());

    do_begin();

    ESP_ERROR_CHECK(setup_flash());

    begin_network();
}

esp_err_t ApplicationBase::setup_flash() {
    ESP_LOGI(TAG, "Setting up flash");

    auto ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    return ret;
}

void ApplicationBase::begin_network() {
    ESP_LOGI(TAG, "Connecting to WiFi");

    _network_connection.on_state_changed([this](auto state) {
        if (state.connected) {
            begin_network_available();
        } else {
            ESP_LOGE(TAG, "Failed to connect to WiFi; restarting");
            esp_restart();
        }
    });

    ESP_ERROR_CHECK(_network_connection.begin(CONFIG_WIFI_PASSWORD));
}

void ApplicationBase::begin_network_available() {
    ESP_LOGI(TAG, "Getting device configuration");

    auto err = _configuration.load();

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get configuration; restarting");
        esp_restart();
        return;
    }

    _log_manager.set_device_entity_id(strdup(_configuration.get_device_entity_id().c_str()));

    if (_configuration.get_enable_ota()) {
        _ota_manager.begin();
    }

    ESP_LOGI(TAG, "Configuration loaded; signalling network available");

    do_network_available();

    ESP_LOGI(TAG, "Connecting to MQTT");

    _mqtt_connection.on_connected_changed([this](auto state) {
        if (state.connected) {
            _queue.enqueue([this]() { begin_after_initialization(); });
        } else {
            ESP_LOGE(TAG, "MQTT connection lost");
            esp_restart();
        }
    });

    _mqtt_connection.set_configuration({
        .mqtt_endpoint = _configuration.get_mqtt_endpoint(),
        .mqtt_username = _configuration.get_mqtt_username(),
        .mqtt_password = _configuration.get_mqtt_password(),
        .device_name = _configuration.get_device_name(),
        .device_entity_id = _configuration.get_device_entity_id(),
    });

    _mqtt_connection.begin();
}

void ApplicationBase::begin_after_initialization() {
    // Log the reset reason.

    auto reset_reason = esp_reset_reason();
    ESP_LOGI(TAG, "esp_reset_reason: %s (%d)", esp_reset_reason_to_name(reset_reason), reset_reason);

    ESP_LOGI(TAG, "Startup complete");

    do_ready();
}

void ApplicationBase::process() {
    _queue.process();
    _device.process();
    _status_led.process();
}
