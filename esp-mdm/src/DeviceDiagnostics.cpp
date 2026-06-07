#include "support.h"

#include "DeviceDiagnostics.h"

LOG_TAG(DeviceDiagnostics);

DeviceDiagnostics::DeviceDiagnostics(NetworkConnection& network_connection, MQTTConnection& mqtt_connection,
                                     Queue& queue)
    : _network_connection(network_connection), _mqtt_connection(mqtt_connection), _queue(queue) {}

void DeviceDiagnostics::begin() {
    // Discovery is re-published on every (re)connect; it is retained and pruned
    // by MQTTConnection. Publishing the state right after lets the entities
    // populate immediately instead of waiting for the first periodic tick.
    _mqtt_connection.on_publish_discovery([this]() {
        publish_discovery();
        publish_state();
    });

    // Periodic refresh so RSSI/uptime stay current even when nothing else
    // changes. Started once; reschedules itself.
    schedule_publish();
}

void DeviceDiagnostics::schedule_publish() {
    _queue.enqueue_delayed(
        [this]() {
            publish_state();
            schedule_publish();
        },
        PUBLISH_INTERVAL_MS);
}

void DeviceDiagnostics::publish_discovery() {
    // All entities read from the dedicated diagnostics topic. The c_str() stays
    // valid for the duration of each (synchronous) publish_sensor_discovery call.
    const auto state_topic = get_state_topic();

    _mqtt_connection.publish_sensor_discovery(
        MQTTDiscovery{
            .name = "WiFi Signal",
            .object_id = "wifi_rssi",
            .entity_category = "diagnostic",
            .device_class = "signal_strength",
        },
        MQTTSensorDiscovery{
            .state_class = "measurement",
            .unit_of_measurement = "dBm",
            .value_template = "{{ value_json.wifi_rssi }}",
            .state_topic = state_topic.c_str(),
            .suggested_display_precision = 0,
        });

    // Cosmetic 0-100% mapping of RSSI. Deliberately no device_class: the
    // signal_strength class rejects "%" as a unit.
    _mqtt_connection.publish_sensor_discovery(
        MQTTDiscovery{
            .name = "WiFi Signal Percent",
            .object_id = "wifi_signal_pct",
            .entity_category = "diagnostic",
        },
        MQTTSensorDiscovery{
            .state_class = "measurement",
            .unit_of_measurement = "%",
            .value_template = "{{ value_json.wifi_pct }}",
            .state_topic = state_topic.c_str(),
        });

    // total_increasing: HA treats the reset to 0 on reboot as a new cycle.
    _mqtt_connection.publish_sensor_discovery(
        MQTTDiscovery{
            .name = "Uptime",
            .object_id = "uptime",
            .entity_category = "diagnostic",
            .device_class = "duration",
        },
        MQTTSensorDiscovery{
            .state_class = "total_increasing",
            .unit_of_measurement = "s",
            .value_template = "{{ value_json.uptime }}",
            .state_topic = state_topic.c_str(),
        });

    _mqtt_connection.publish_sensor_discovery(
        MQTTDiscovery{
            .name = "IP Address",
            .object_id = "ip_address",
            .icon = "mdi:ip-network",
            .entity_category = "diagnostic",
        },
        MQTTSensorDiscovery{
            .value_template = "{{ value_json.ip_address }}",
            .state_topic = state_topic.c_str(),
        });

    _mqtt_connection.publish_sensor_discovery(
        MQTTDiscovery{
            .name = "SSID",
            .object_id = "ssid",
            .icon = "mdi:wifi",
            .entity_category = "diagnostic",
        },
        MQTTSensorDiscovery{
            .value_template = "{{ value_json.ssid }}",
            .state_topic = state_topic.c_str(),
        });

    _mqtt_connection.publish_sensor_discovery(
        MQTTDiscovery{
            .name = "BSSID",
            .object_id = "bssid",
            .icon = "mdi:access-point",
            .entity_category = "diagnostic",
        },
        MQTTSensorDiscovery{
            .value_template = "{{ value_json.bssid }}",
            .state_topic = state_topic.c_str(),
        });

    _mqtt_connection.publish_sensor_discovery(
        MQTTDiscovery{
            .name = "MAC Address",
            .object_id = "mac_address",
            .icon = "mdi:network-outline",
            .entity_category = "diagnostic",
        },
        MQTTSensorDiscovery{
            .value_template = "{{ value_json.mac_address }}",
            .state_topic = state_topic.c_str(),
        });
}

void DeviceDiagnostics::publish_state() {
    const auto wifi = _network_connection.get_wifi_info();

    auto root = cJSON_CreateObject();
    ESP_ASSERT_CHECK(root);
    DEFER(cJSON_Delete(root));

    if (wifi.associated) {
        cJSON_AddNumberToObject(root, "wifi_rssi", wifi.rssi);

        // pct = clamp(2 * (rssi + 100), 0, 100).
        int pct = 2 * (static_cast<int>(wifi.rssi) + 100);
        if (pct < 0) {
            pct = 0;
        } else if (pct > 100) {
            pct = 100;
        }
        cJSON_AddNumberToObject(root, "wifi_pct", pct);

        cJSON_AddStringToObject(root, "ssid", wifi.ssid.c_str());
        cJSON_AddStringToObject(root, "bssid", wifi.bssid.c_str());
    }

    cJSON_AddNumberToObject(root, "uptime", static_cast<double>(esp_timer_get_time() / 1000000));

    const auto ip = _network_connection.get_ip_address();
    if (!ip.empty()) {
        cJSON_AddStringToObject(root, "ip_address", ip.c_str());
    }

    cJSON_AddStringToObject(root, "mac_address", _network_connection.get_mac_address().c_str());

    auto json = cJSON_PrintUnformatted(root);
    // Retained: this topic is the source of truth for the diagnostic entities.
    // publish() is a no-op while the transport is down, so an offline tick is
    // harmless.
    _mqtt_connection.publish(get_state_topic(), json, 1, true);
    cJSON_free(json);
}

std::string DeviceDiagnostics::get_state_topic() { return _mqtt_connection.get_device_topic("diagnostics"); }
