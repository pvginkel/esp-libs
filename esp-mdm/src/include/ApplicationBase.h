#pragma once

#include "Callback.h"
#include "LogManager.h"
#include "MDMConfiguration.h"
#include "MQTTConnection.h"
#include "NetworkConnection.h"
#include "OTAManager.h"
#include "Queue.h"

class ApplicationBase {
    NetworkConnection _network_connection;
    MQTTConnection _mqtt_connection;
    OTAManager _ota_manager;
    Queue _queue;
    LogManager _log_manager;
    MDMConfiguration _mdm_configuration;
    Callback<void> _begin;
    Callback<void> _network_available;
    Callback<void> _ready;
    std::string _access_token;
    std::string _authorization;
    uint32_t _token_expires_at{};
    std::string _device_name;
    std::string _device_entity_id;
    bool _enable_ota{};

public:
    ApplicationBase();

    void begin(bool silent);
    void process();

    void on_begin(std::function<void()> func) { _begin.add(func); }
    void on_network_available(std::function<void()> func) { _network_available.add(func); }
    void on_ready(std::function<void()> func) { _ready.add(func); }

protected:
    virtual void do_begin() { _begin.call(); }
    virtual void do_network_available() { _network_available.call(); }
    virtual void do_ready() { _ready.call(); }

private:
    esp_err_t setup_flash();
    void begin_network();
    void begin_network_available();
    esp_err_t ensure_access_token();
    esp_err_t load_device_configuration();
    void begin_after_initialization();
};
