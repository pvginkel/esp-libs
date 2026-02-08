#pragma once

#include "Callback.h"
#include "LogManager.h"
#include "MDMConfiguration.h"
#include "MQTTConnection.h"
#include "NetworkConnection.h"
#include "Queue.h"

class ApplicationBase {
    NetworkConnection _network_connection;
    MQTTConnection _mqtt_connection;
    Queue _queue;
    LogManager _log_manager;
    MDMConfiguration _mdm_configuration;
    Callback<void> _begin;
    Callback<void> _network_available;
    Callback<void> _network_connection_failed;
    Callback<void> _ready;
    Callback<cJSON*> _configuration_loaded;
    Callback<void> _process;
    std::string _access_token;
    std::string _authorization;
    int64_t _token_expires_at{};
    std::string _device_name;
    std::string _device_entity_id;
    bool _enable_ota{};
    bool _silent_startup;

public:
    ApplicationBase();

    void begin();
    void process();

    const std::string& get_authorization();
    Queue& get_queue() { return _queue; }
    MQTTConnection& get_mqtt_connection() { return _mqtt_connection; }
    bool is_silent_startup() { return _silent_startup; }

    void on_begin(std::function<void()> func) { _begin.add(func); }
    void on_network_available(std::function<void()> func) { _network_available.add(func); }
    void on_network_connection_failed(std::function<void()> func) { _network_connection_failed.add(func); }
    void on_ready(std::function<void()> func) { _ready.add(func); }
    void on_configuration_loaded(std::function<void(cJSON*)> func) { _configuration_loaded.add(func); }

protected:
    virtual void do_begin() { _begin.call(); }
    virtual void do_network_available() { _network_available.call(); }
    virtual void do_network_connection_failed() { _network_connection_failed.call(); }
    virtual void do_ready() { _ready.call(); }
    virtual void do_configuration_loaded(cJSON* data) { _configuration_loaded.call(data); }
    virtual void do_process() { _process.call(); }

private:
    esp_err_t setup_flash();
    void begin_network();
    void begin_network_available();
    esp_err_t ensure_access_token();
    esp_err_t install_firmware_update();
    esp_err_t load_device_configuration();
    esp_err_t parse_device_configuration(cJSON* data);
    void setup_mqtt_subscriptions();
    bool is_iotsupport_message_for_us(const std::string& data, const char* message_type);
    void handle_iotsupport_update(const std::string& data, const char* update_type);
    esp_err_t handle_iotsupport_provisioning();
    void begin_after_initialization();
};
