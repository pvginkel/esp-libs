#pragma once

#include <stdint.h>

#include "Callback.h"
#include "Queue.h"
#include "esp_event.h"
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
    int _connect_attempts;
    int _attempt{};
    bool _have_sntp_synced{};

public:
    NetworkConnection(Queue *synchronizationQueue, int connect_attempts);

    void begin(const char *ssid, const char *password);
    void on_state_changed(std::function<void(NetworkConnectionState)> func) { _state_changed.add(func); }

private:
    void event_handler(esp_event_base_t eventBase, int32_t eventId, void *eventData);
    void setup_sntp();
};
