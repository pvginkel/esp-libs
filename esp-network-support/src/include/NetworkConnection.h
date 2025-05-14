#pragma once

#include <stdint.h>

#include <string>

#include "Callback.h"
#include "Queue.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

struct NetworkConnectionState {
    bool connected;
    uint8_t errorReason;
};

class NetworkConnection {
    static NetworkConnection *_instance;
    Queue *_synchronization_queue;
    EventGroupHandle_t _wifi_event_group;
    Callback<NetworkConnectionState> _state_changed;
    int _attempt{};
    bool _have_sntp_synced{};
    esp_netif_t *_wifi_interface{};

public:
    NetworkConnection(Queue *synchronizationQueue);

    void begin(const char *password);
    void on_state_changed(std::function<void(NetworkConnectionState)> func) { _state_changed.add(func); }
    std::string get_ip_address();

private:
    void event_handler(esp_event_base_t eventBase, int32_t eventId, void *eventData);
    void setup_sntp();
};
