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

struct WiFiInfo {
    // False when the station is not currently associated with an AP; the
    // remaining fields are then unspecified (RSSI is only meaningful in station
    // mode).
    bool associated;
    int8_t rssi;
    std::string ssid;
    std::string bssid;
};

class NetworkConnection {
    static NetworkConnection* _instance;
    Queue* _synchronization_queue;
    EventGroupHandle_t _wifi_event_group;
    Callback<NetworkConnectionState> _state_changed;
    int _attempt{};
    bool _have_connected{};
    bool _have_sntp_synced{};
    esp_netif_t* _wifi_interface{};

public:
    NetworkConnection(Queue* synchronizationQueue);

    esp_err_t begin(const char* ssid, const char* password, int8_t max_tx_power);
    void on_state_changed(std::function<void(NetworkConnectionState)> func) { _state_changed.add(func); }
    std::string get_ip_address();
    // Live snapshot of the station's association (RSSI/SSID/BSSID) in a single
    // esp_wifi_sta_get_ap_info() call so the values are mutually consistent.
    WiFiInfo get_wifi_info();
    // The device's own station-interface MAC, formatted AA:BB:CC:DD:EE:FF.
    std::string get_mac_address();

private:
    void event_handler(esp_event_base_t eventBase, int32_t eventId, void* eventData);
    void setup_sntp();
};
