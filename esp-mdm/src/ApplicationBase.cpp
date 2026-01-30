#include "support.h"

#include "ApplicationBase.h"

#include "OTAManager.h"
#include "cJSON.h"
#include "driver/i2c.h"
#include "http_support.h"
#include "nvs_flash.h"

#define DEVICE_CONFIGURATION_URL "api/iot/config"
#define DEVICE_PROVISIONING_URL "api/iot/provisioning"
#define DEVICE_FIRMWARE_URL "api/iot/firmware"

LOG_TAG(ApplicationBase);

ApplicationBase::ApplicationBase() : _network_connection(&_queue), _mqtt_connection(&_queue) {}

void ApplicationBase::begin(bool silent) {
    ESP_ERROR_CHECK(setup_flash());

    ESP_LOGI(TAG, "Loading provisioning data");

    ESP_ERROR_CHECK(_mdm_configuration.load());

    do_begin();

    begin_network();

    ESP_LOGI(TAG, "Setting up the log manager");

    ESP_ERROR_CHECK(_log_manager.begin(_mdm_configuration.get_logging_url()));
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

    ESP_ERROR_CHECK(_network_connection.begin(_mdm_configuration.get_wifi_ssid().c_str(),
                                              _mdm_configuration.get_wifi_password().c_str()));
}

void ApplicationBase::begin_network_available() {
    ESP_LOGI(TAG, "Getting device configuration");

    ESP_ERROR_CHECK(load_device_configuration());

    _log_manager.set_device_entity_id(_device_entity_id);

    ESP_LOGI(TAG, "Checking for firmware update");

    ESP_ERROR_CHECK(install_firmware_update());

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
        .mqtt_endpoint = _mdm_configuration.get_mqtt_url(),
        .mqtt_username = _access_token,
        .mqtt_password = "x",
        .device_name = _device_name,
        .device_entity_id = _device_entity_id,
    });

    _mqtt_connection.begin();
}

esp_err_t ApplicationBase::ensure_access_token() {
    // Check if we have a valid token (with 30 second buffer).
    auto now = esp_get_millis();
    if (!_access_token.empty() && now + 30000 < _token_expires_at) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "Requesting access token from %s", _mdm_configuration.get_token_url().c_str());

    // Build the POST body.
    auto body = strformat("grant_type=client_credentials&scope=openid+profile+email&client_id=%s&client_secret=%s",
                          _mdm_configuration.get_client_id(), _mdm_configuration.get_client_secret());

    // Make the token request.
    esp_http_client_config_t config = {
        .url = _mdm_configuration.get_token_url().c_str(),
        .timeout_ms = CONFIG_NETWORK_RECEIVE_TIMEOUT,
    };

    auto client = esp_http_client_init(&config);
    ESP_RETURN_ON_FALSE(client, ESP_FAIL, TAG, "Failed to init HTTP client");
    DEFER(esp_http_client_cleanup(client));

    ESP_ERROR_RETURN(esp_http_client_set_method(client, HTTP_METHOD_POST));
    ESP_ERROR_RETURN(esp_http_client_set_header(client, "Content-Type", "application/x-www-form-urlencoded"));
    ESP_ERROR_RETURN(esp_http_client_open(client, body.length()));

    ESP_ASSERT_RETURN(esp_http_client_write(client, body.c_str(), body.length()) >= 0, ESP_FAIL);

    auto length = esp_http_client_fetch_headers(client);
    if (length < 0) {
        ESP_ERROR_RETURN(esp_err_t(-length));
    }

    std::string response;
    ESP_ERROR_RETURN(esp_http_get_response(client, response, 4096));

    // Parse the response.
    auto data = cJSON_Parse(response.c_str());
    DEFER(cJSON_Delete(data));

    ESP_RETURN_ON_FALSE(data, ESP_ERR_INVALID_RESPONSE, TAG, "Failed to parse token response JSON");

    auto access_token_item = cJSON_GetObjectItemCaseSensitive(data, "access_token");
    ESP_RETURN_ON_FALSE(cJSON_IsString(access_token_item) && access_token_item->valuestring, ESP_ERR_INVALID_RESPONSE,
                        TAG, "Cannot get access_token property");

    auto expires_in_item = cJSON_GetObjectItemCaseSensitive(data, "expires_in");
    ESP_RETURN_ON_FALSE(cJSON_IsNumber(expires_in_item), ESP_ERR_INVALID_RESPONSE, TAG,
                        "Cannot get expires_in property");

    _access_token = access_token_item->valuestring;
    _authorization = "Bearer " + _access_token;
    _token_expires_at = esp_get_millis() + int64_t(expires_in_item->valuedouble * 1000);

    ESP_LOGI(TAG, "Access token acquired, expires in %.0f seconds", expires_in_item->valuedouble);

    return ESP_OK;
}

esp_err_t ApplicationBase::install_firmware_update() {
    if (!_enable_ota) {
        ESP_LOGI(TAG, "OTA disabled.");
        return ESP_OK;
    }

    const auto firmware_url = _mdm_configuration.get_base_url() + DEVICE_FIRMWARE_URL;

    ESP_LOGI(TAG, "Getting firmware update %s", firmware_url.c_str());

    ESP_ERROR_RETURN(ensure_access_token());

    OTAManager ota_manager;

    if (ota_manager.install_update(firmware_url, _authorization)) {
        ESP_LOGI(TAG, "New firmware installed. Restarting the device.");
        esp_restart();
    }

    return ESP_OK;
}

esp_err_t ApplicationBase::load_device_configuration() {
    const auto url = _mdm_configuration.get_base_url() + DEVICE_CONFIGURATION_URL;
    esp_http_client_config_t config = {
        .url = url.c_str(),
        .timeout_ms = CONFIG_NETWORK_RECEIVE_TIMEOUT,
        .buffer_size_tx = 4096,
    };

    ESP_LOGI(TAG, "Getting device configuration from %s", config.url);

    ESP_ERROR_RETURN(ensure_access_token());

    auto client = esp_http_client_init(&config);
    ESP_RETURN_ON_FALSE(client, ESP_FAIL, TAG, "Failed to init HTTP client");
    DEFER(esp_http_client_cleanup(client));

    ESP_ERROR_RETURN(esp_http_client_set_header(client, "Authorization", _authorization.c_str()));
    ESP_ERROR_RETURN(esp_http_client_open(client, 0));

    auto length = esp_http_client_fetch_headers(client);
    if (length < 0) {
        ESP_ERROR_RETURN(esp_err_t(-length));
    }

    cJSON* data = nullptr;
    ESP_ERROR_RETURN(esp_http_get_json(client, data, 128 * 1024));
    DEFER(cJSON_Delete(data));

    ESP_ERROR_RETURN(parse_device_configuration(data));

    ESP_LOGI(TAG, "Signalling configuration loaded");

    do_configuration_loaded(data);

    return ESP_OK;
}

esp_err_t ApplicationBase::parse_device_configuration(cJSON* data) {
    auto device_name_item = cJSON_GetObjectItemCaseSensitive(data, "deviceName");
    ESP_RETURN_ON_FALSE(cJSON_IsString(device_name_item) && device_name_item->valuestring, ESP_ERR_INVALID_ARG, TAG,
                        "Cannot get deviceName property");

    _device_name = device_name_item->valuestring;

    ESP_LOGI(TAG, "Device name: %s", _device_name.c_str());

    auto device_entity_id_item = cJSON_GetObjectItemCaseSensitive(data, "deviceEntityId");
    ESP_RETURN_ON_FALSE(cJSON_IsString(device_entity_id_item) && device_entity_id_item->valuestring,
                        ESP_ERR_INVALID_ARG, TAG, "Cannot get deviceEntityIdItem property");

    _device_entity_id = device_entity_id_item->valuestring;

    ESP_LOGI(TAG, "Device entity ID: %s", _device_entity_id.c_str());

    auto enable_ota_item = cJSON_GetObjectItemCaseSensitive(data, "enableOTA");
    if (enable_ota_item != nullptr) {
        if (!cJSON_IsBool(enable_ota_item)) {
            ESP_LOGE(TAG, "Cannot get enableOTAItem property");
            return ESP_ERR_INVALID_ARG;
        }

        _enable_ota = cJSON_IsTrue(enable_ota_item);
    }

    ESP_LOGI(TAG, "Enable OTA: %s", _enable_ota ? "yes" : "no");

    return ESP_OK;
}

void ApplicationBase::begin_after_initialization() {
    setup_mqtt_subscriptions();

    // Log the reset reason.

    auto reset_reason = esp_reset_reason();
    ESP_LOGI(TAG, "esp_reset_reason: %s (%d)", esp_reset_reason_to_name(reset_reason), reset_reason);

    ESP_LOGI(TAG, "Startup complete");

    do_ready();
}

void ApplicationBase::setup_mqtt_subscriptions() {
    _mqtt_connection.subscribe("iotsupport/updates/config",
                               [this](const std::string& data) { handle_iotsupport_update(data, "config"); });

    if (_enable_ota) {
        _mqtt_connection.subscribe("iotsupport/updates/firmware",
                                   [this](const std::string& data) { handle_iotsupport_update(data, "firmware"); });
    }

    _mqtt_connection.subscribe("iotsupport/updates/provisioning", [this](const std::string& data) {
        if (!is_iotsupport_message_for_us(data, "provisioning")) {
            return;
        }

        auto err = handle_iotsupport_provisioning();
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Provisioning failed: %s", esp_err_to_name(err));
            return;
        }

        ESP_LOGI(TAG, "Provisioning complete; restarting");
        esp_restart();
    });
}

bool ApplicationBase::is_iotsupport_message_for_us(const std::string& data, const char* message_type) {
    auto json = cJSON_Parse(data.c_str());
    if (!json) {
        ESP_LOGE(TAG, "Failed to parse %s message JSON", message_type);
        return false;
    }
    DEFER(cJSON_Delete(json));

    auto client_id_item = cJSON_GetObjectItemCaseSensitive(json, "client_id");
    if (!cJSON_IsString(client_id_item) || !client_id_item->valuestring) {
        ESP_LOGE(TAG, "Cannot get client_id from %s message", message_type);
        return false;
    }

    return _mdm_configuration.get_client_id() == client_id_item->valuestring;
}

void ApplicationBase::handle_iotsupport_update(const std::string& data, const char* update_type) {
    if (is_iotsupport_message_for_us(data, update_type)) {
        ESP_LOGI(TAG, "Received %s update for this device; restarting", update_type);
        esp_restart();
    }
}

esp_err_t ApplicationBase::handle_iotsupport_provisioning() {
    ESP_ERROR_RETURN(ensure_access_token());

    const auto url = _mdm_configuration.get_base_url() + DEVICE_PROVISIONING_URL;
    esp_http_client_config_t config = {
        .url = url.c_str(),
        .timeout_ms = CONFIG_NETWORK_RECEIVE_TIMEOUT,
        .buffer_size_tx = 4096,
    };

    ESP_LOGI(TAG, "Getting provisioning data from %s", config.url);

    auto client = esp_http_client_init(&config);
    ESP_RETURN_ON_FALSE(client, ESP_FAIL, TAG, "Failed to init HTTP client");
    DEFER(esp_http_client_cleanup(client));

    ESP_ERROR_RETURN(esp_http_client_set_header(client, "Authorization", _authorization.c_str()));
    ESP_ERROR_RETURN(esp_http_client_open(client, 0));

    auto length = esp_http_client_fetch_headers(client);
    if (length < 0) {
        return (esp_err_t)-length;
    }

    cJSON* provisioning_data = nullptr;
    ESP_ERROR_RETURN(esp_http_get_json(client, provisioning_data, 128 * 1024));
    DEFER(cJSON_Delete(provisioning_data));

    ESP_ERROR_RETURN(_mdm_configuration.save_provisioning(provisioning_data));

    return ESP_OK;
}

void ApplicationBase::process() {
    _queue.process();

    do_process();
}
