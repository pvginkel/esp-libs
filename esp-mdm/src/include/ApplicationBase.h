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
    void begin_after_initialization();
};
