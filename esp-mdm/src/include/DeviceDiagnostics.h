#pragma once

#include <cstdint>
#include <string>

#include "MQTTConnection.h"
#include "NetworkConnection.h"
#include "Queue.h"

// Publishes WiFi / connection diagnostic sensors (RSSI, signal %, uptime, IP,
// SSID, BSSID, MAC) to Home Assistant via MQTT discovery. Owned by
// ApplicationBase so every device built on this stack exposes them without any
// per-application code.
//
// Values are carried on a dedicated retained "<prefix>/diagnostics" topic on a
// fixed cadence, keeping the diagnostics self-contained and independent of each
// application's own state document. Availability still derives from the shared
// device state topic, so the entities go unavailable with the device.
class DeviceDiagnostics {
    static constexpr uint32_t PUBLISH_INTERVAL_MS = 60000;

    NetworkConnection& _network_connection;
    MQTTConnection& _mqtt_connection;
    Queue& _queue;

public:
    DeviceDiagnostics(NetworkConnection& network_connection, MQTTConnection& mqtt_connection, Queue& queue);

    // Registers discovery (re-published on every reconnect) and starts the
    // periodic state publish. Call once.
    void begin();

private:
    void publish_discovery();
    void publish_state();
    void schedule_publish();
    std::string get_state_topic();
};
