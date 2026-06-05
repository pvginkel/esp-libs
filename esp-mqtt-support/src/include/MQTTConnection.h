#pragma once

#include <cstdint>
#include <map>
#include <set>
#include <string>

#include "Callback.h"
#include "Mutex.h"
#include "Queue.h"
#include "Signal.h"
#include "Span.h"
#include "cJSON.h"
#include "mqtt_client.h"

struct MQTTConnectionState {
    bool connected;
};

struct MQTTDeviceConfiguration {
    std::string manufacturer;
    std::string model;
    std::string model_id;
};

struct MQTTConfiguration {
    std::string mqtt_endpoint;
    std::string mqtt_username;
    std::string mqtt_password;
    std::string device_name;
    std::string device_entity_id;
    MQTTDeviceConfiguration device;
};

struct MQTTDiscovery {
    const char* name;
    const char* object_id;
    const char* icon;
    const char* entity_category;
    const char* device_class;
    const char* subdevice_name;
    const char* subdevice_id;
    bool enabled_by_default = true;
};

struct MQTTSensorDiscovery {
    const char* state_class;
    const char* unit_of_measurement;
    const char* value_template;
};

struct MQTTSwitchDiscovery {
    const char* value_template;
};

struct MQTTBinarySensorDiscovery {
    const char* value_template;
};

struct MQTTNumberDiscovery {
    const char* unit_of_measurement;
    const char* value_template;
    double min;
    double max;
    double step;
};

struct MQTTDeviceAutomationDiscovery {
    const char* subdevice_name;
    const char* subdevice_id;
    const char* trigger_name;
    const char* trigger_value;
};

class MQTTConnection {
    static constexpr double DEFAULT_SETPOINT = 19;

    static std::string get_device_id();

    Queue* _queue;
    std::string _device_id;
    MQTTConfiguration _configuration;
    std::string _topic_prefix;
    esp_mqtt_client_handle_t _client{};
    Callback<MQTTConnectionState> _connected_changed;
    Callback<void> _publish_discovery;
    Callback<cJSON*> _create_configuration;
    std::map<std::string, std::function<void(const std::string&)>> _command_callbacks;
    std::map<std::string, std::function<void(const std::string&)>> _topic_callbacks;
    std::map<int, std::function<void()>> _subscribed_callbacks;
    std::set<std::string> _published_discovery_topics;
    bool _connected{};
    // Back-pressure for QoS>0 publishes. _inflight maps the msg_id of each
    // unacknowledged QoS 1/2 PUBLISH to the timestamp (us) it was sent. Producers
    // block once the number in flight plus _reserved reaches
    // CONFIG_MQTT_MAX_INFLIGHT_QOS, and are released as the MQTT event task drains
    // acknowledgements/deletions or on disconnect. _reserved counts slots claimed
    // by producers that have not yet learned their msg_id (the window around the
    // esp_mqtt_client_publish call). Entries older than CONFIG_MQTT_INFLIGHT_TTL_MS
    // are reclaimed unconditionally, healing slots for messages esp-mqtt drops
    // without ever reporting an event. _transport_connected tracks the raw
    // connection (CONNECTED..DISCONNECTED), unlike _connected which only flips true
    // once the initial subscribe completes.
    Mutex _inflight_mutex;
    Signal _slot_available;
    std::map<int, int64_t> _inflight;
    int _reserved{};
    bool _transport_connected{};

public:
    MQTTConnection(Queue* queue);

    void set_configuration(MQTTConfiguration configuration) { _configuration = configuration; }
    void begin();
    bool is_connected() { return _connected; }
    bool publish(const std::string& topic, const std::string& payload, int qos = 1, bool retain = false);
    void send_state();
    void send_state(cJSON* data);
    void send_trigger(const char* name, const char* value);
    void on_connected_changed(std::function<void(MQTTConnectionState)> func) { _connected_changed.add(func); }
    void on_publish_discovery(std::function<void()> func) { _publish_discovery.add(func); }
    void on_create_configuration(std::function<void(cJSON*)> func) { _create_configuration.add(func); }
    void subscribe(const std::string& topic, std::function<void(const std::string&)> callback);
    void register_callback(const char* object_id, std::function<void(const std::string&)> callback);
    void publish_button_discovery(MQTTDiscovery metadata, std::function<void()> command_func);
    void publish_sensor_discovery(MQTTDiscovery metadata, MQTTSensorDiscovery component_metadata);
    void publish_switch_discovery(MQTTDiscovery metadata, MQTTSwitchDiscovery component_metadata,
                                  std::function<void(bool)> command_func);
    void publish_binary_sensor_discovery(MQTTDiscovery metadata, MQTTBinarySensorDiscovery component_metadata);
    void publish_number_discovery(MQTTDiscovery metadata, MQTTNumberDiscovery component_metadata,
                                  std::function<void(const std::string&)> command_func);
    void publish_device_automation(MQTTDeviceAutomationDiscovery metadata);

private:
    void event_handler(esp_event_base_t eventBase, int32_t eventId, void* eventData);
    void handle_connected();
    void handle_data(esp_mqtt_event_handle_t event);
    int subscribe(const std::string& topic);
    void subscribe(const std::string& topic, std::function<void()> subscribed_func);
    void unsubscribe(const std::string& topic);
    void publish_configuration();
    void publish_json(cJSON* root, const std::string& topic, bool retain);
    void publish_discovery(const char* component, const MQTTDiscovery& metadata,
                           std::function<void(cJSON* json, const char* object_id)> func);
    bool handle_discovery_prune(const std::string& topic, bool empty_message);
    std::string get_firmware_version();
    int publish_with_backpressure(const char* topic, const char* data, int len, int qos, bool retain);
    void set_transport_connected(bool connected);
    void release_inflight_id(int msg_id);
    int purge_expired_inflight(int64_t now);
    void add_device_metadata(cJSON* root, const char* subdevice_id, const char* subdevice_name);
};
