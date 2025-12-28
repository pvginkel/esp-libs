#pragma once

#include <string>

#include "Callback.h"
#include "Queue.h"
#include "Span.h"
#include "cJSONSupport.h"
#include "mqtt_client.h"

struct MQTTConnectionState {
    bool connected;
};

struct MQTTConfiguration {
    std::string mqtt_endpoint;
    std::string mqtt_username;
    std::string mqtt_password;
    std::string device_name;
    std::string device_entity_id;
};

struct MQTTData {
    const std::string& topic;
    const std::string& data;
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
    Callback<MQTTData&> _set_message;

public:
    MQTTConnection(Queue* queue);

    void set_configuration(MQTTConfiguration configuration) { _configuration = configuration; }
    void begin();
    bool is_connected() { return !!_client; }
    void send_state(cJSON* data);
    void on_connected_changed(std::function<void(MQTTConnectionState)> func) { _connected_changed.add(func); }
    void on_publish_discovery(std::function<void()> func) { _publish_discovery.add(func); }
    void on_set_message(std::function<void(MQTTData&)> func) { _set_message.add(func); }
    void publish_button_discovery(const char* name, const char* object_id, const char* icon,
                                  const char* entity_category, const char* device_class);
    void publish_sensor_discovery(const char* name, const char* object_id, const char* icon,
                                  const char* entity_category, const char* device_class, const char* state_class,
                                  const char* unit_of_measurement, const char* value_template);
    void publish_switch_discovery(const char* name, const char* object_id, const char* icon,
                                  const char* entity_category, const char* device_class, const char* value_template);

private:
    void event_handler(esp_event_base_t eventBase, int32_t eventId, void* eventData);
    void handle_connected();
    void handle_data(esp_mqtt_event_handle_t event);
    void subscribe(const std::string& topic);
    void unsubscribe(const std::string& topic);
    void publish_configuration();
    void publish_json(cJSON* root, const std::string& topic, bool retain);
    cJSON_Data create_discovery(const char* component, const char* name, const char* object_id,
                                const char* subdevice_name, const char* subdevice_id, const char* icon,
                                const char* entity_category, const char* device_class, bool enabled_by_default);
    std::string get_firmware_version();
};
