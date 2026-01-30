#include "support.h"

#include "MQTTConnection.h"

#include <charconv>

#include "defer.h"
#include "esp_mac.h"
#include "esp_ota_ops.h"

LOG_TAG(MQTTConnection);

#define LAST_WILL_MESSAGE "{\"online\": false}"

#define QOS_MAX_ONE 0      // Send at most one.
#define QOS_MIN_ONE 1      // Send at least one.
#define QOS_EXACTLY_ONE 2  // Send exactly one.

#define MAXIMUM_PACKET_SIZE 4096

MQTTConnection::MQTTConnection(Queue* queue) : _queue(queue), _device_id(get_device_id()) {}

void MQTTConnection::begin() {
    esp_log_level_set("mqtt5_client", ESP_LOG_WARN);

    esp_mqtt5_connection_property_config_t connect_property = {
        .session_expiry_interval = 10,
        .maximum_packet_size = MAXIMUM_PACKET_SIZE,
        .receive_maximum = 65535,
        .topic_alias_maximum = 2,
        .request_resp_info = true,
        .request_problem_info = true,
        .will_delay_interval = 0,
        .message_expiry_interval = 10,
        .payload_format_indicator = true,
    };

    _topic_prefix = CONFIG_MQTT_TOPIC_PREFIX "/" + _device_id + "/";

    const auto state_topic = _topic_prefix + "state";

    esp_mqtt_client_config_t config = {
        .broker =
            {
                .address =
                    {
                        .uri = _configuration.mqtt_endpoint.c_str(),
                    },
            },
        .session =
            {
                .last_will =
                    {
                        .topic = state_topic.c_str(),
                        .msg = LAST_WILL_MESSAGE,
                        .qos = QOS_MIN_ONE,
                        .retain = true,
                    },
                .protocol_ver = MQTT_PROTOCOL_V_5,
            },
        .network =
            {
                .disable_auto_reconnect = false,
            },
        .buffer =
            {
                .size = MAXIMUM_PACKET_SIZE,
            },
    };

    if (_configuration.mqtt_username.length()) {
        config.credentials.username = _configuration.mqtt_username.c_str();
        config.credentials.authentication.password = _configuration.mqtt_password.c_str();
    }

    _client = esp_mqtt_client_init(&config);

    esp_mqtt5_client_set_connect_property(_client, &connect_property);

    esp_mqtt_client_register_event(
        _client, MQTT_EVENT_ANY,
        [](auto eventHandlerArg, auto eventBase, auto eventId, auto eventData) {
            ((MQTTConnection*)eventHandlerArg)->event_handler(eventBase, eventId, eventData);
        },
        this);

    esp_mqtt_client_start(_client);
}

std::string MQTTConnection::get_device_id() {
    uint8_t mac[6];

    ESP_ERROR_CHECK(esp_read_mac(mac, ESP_MAC_WIFI_STA));

    return strformat("0x%02x%02x%02x%02x%02x%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

void MQTTConnection::event_handler(esp_event_base_t eventBase, int32_t eventId, void* eventData) {
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRIi32, eventBase, eventId);
    auto event = (esp_mqtt_event_handle_t)eventData;

    ESP_LOGD(TAG, "Free heap size is %" PRIu32 ", minimum %" PRIu32, esp_get_free_heap_size(),
             esp_get_minimum_free_heap_size());

    switch ((esp_mqtt_event_id_t)eventId) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT connected");
            // On connect we're publishing a large number of messages for metadata.
            // We need to do this outside of the MQTT loop because otherwise we
            // wouldn't be able to process in flight ACKs.
            _queue->enqueue([this]() { handle_connected(); });
            break;

        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT disconnected");

            _connected_changed.queue(_queue, {false});
            break;

        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "MQTT subscribed error %d", (int)event->error_handle->error_type);
            break;

        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI(TAG, "MQTT unsubscribed");
            break;

        case MQTT_EVENT_PUBLISHED:
            break;

        case MQTT_EVENT_DATA:
            handle_data(event);
            break;

        case MQTT_EVENT_ERROR:
            ESP_LOGI(TAG, "MQTT return code is %d", event->error_handle->connect_return_code);
            if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
                if (event->error_handle->esp_tls_last_esp_err) {
                    ESP_LOGI(TAG, "reported from esp-tls");
                }
                if (event->error_handle->esp_tls_stack_err) {
                    ESP_LOGI(TAG, "reported from tls stack");
                }
                if (event->error_handle->esp_transport_sock_errno) {
                    ESP_LOGI(TAG, "captured as transport's socket errno");
                }
                ESP_LOGI(TAG, "Last errno string (%s)", strerror(event->error_handle->esp_transport_sock_errno));
            }
            break;

        default:
            ESP_LOGD(TAG, "Other event id: %d", event->event_id);
            break;
    }
}

void MQTTConnection::handle_connected() {
    subscribe(_topic_prefix + "set/#");

    publish_configuration();

    _published_discovery_topics.clear();
    _publish_discovery.call();

    auto discovery_topic = strformat("homeassistant/+/%s/+/config", _device_id);
    subscribe(discovery_topic);
    _queue->enqueue_delayed([this, discovery_topic]() { unsubscribe(discovery_topic); }, 60000);

    _connected_changed.call({true});
}

void MQTTConnection::handle_data(esp_mqtt_event_handle_t event) {
    // We don't support message chunking.
    ESP_ASSERT_CHECK(!event->current_data_offset);

    if (!event->topic_len) {
        ESP_LOGW(TAG, "Handling data without topic");
        return;
    }

    auto topic = std::string(event->topic, event->topic_len);

    if (handle_discovery_prune(topic, !event->data_len)) {
        return;
    }

    // Check for custom topic subscriptions.
    auto topic_it = _topic_callbacks.find(topic);
    if (topic_it != _topic_callbacks.end()) {
        auto data = std::string(event->data, event->data_len);
        topic_it->second(data);
        return;
    }

    if (!topic.starts_with(_topic_prefix)) {
        ESP_LOGE(TAG, "Unexpected topic %s topic len %d data len %d", topic.c_str(), event->topic_len, event->data_len);
        return;
    }

    auto sub_topic = topic.substr(_topic_prefix.length());

    if (!sub_topic.starts_with("set/")) {
        ESP_LOGE(TAG, "Unknown topic %s", topic.c_str());
        return;
    }

    auto object_id = sub_topic.substr(4);
    auto it = _command_callbacks.find(object_id);
    if (it != _command_callbacks.end()) {
        auto data = std::string(event->data, event->data_len);
        it->second(data);
    } else {
        ESP_LOGW(TAG, "No callback registered for object_id '%s'", object_id.c_str());
    }
}

void MQTTConnection::subscribe(const std::string& topic) {
    ESP_LOGI(TAG, "Subscribing to topic %s", topic.c_str());

    ESP_ASSERT_CHECK(esp_mqtt_client_subscribe(_client, topic.c_str(), 0) >= 0);
}

void MQTTConnection::subscribe(const std::string& topic, std::function<void(const std::string&)> callback) {
    _topic_callbacks[topic] = std::move(callback);

    subscribe(topic);
}

void MQTTConnection::unsubscribe(const std::string& topic) {
    ESP_LOGI(TAG, "Unsubscribing from topic %s", topic.c_str());

    ESP_ASSERT_CHECK(esp_mqtt_client_unsubscribe(_client, topic.c_str()) >= 0);
}

void MQTTConnection::publish_configuration() {
    ESP_LOGI(TAG, "Publishing configuration information");

    auto uniqueIdentifier = strformat("%s_%s", CONFIG_MQTT_TOPIC_PREFIX, _device_id);

    auto root = cJSON_CreateObject();
    DEFER(cJSON_Delete(root));

    cJSON_AddStringToObject(root, "unique_id", uniqueIdentifier.c_str());

    auto device = cJSON_AddObjectToObject(root, "device");
    cJSON_AddStringToObject(device, "manufacturer", CONFIG_MQTT_DEVICE_MANUFACTURER);
    cJSON_AddStringToObject(device, "model", CONFIG_MQTT_DEVICE_MODEL);
    cJSON_AddStringToObject(device, "name", _configuration.device_name.c_str());
    cJSON_AddStringToObject(device, "firmware_version", get_firmware_version().c_str());

    publish_json(root, _topic_prefix + "configuration", true);
}

void MQTTConnection::publish_json(cJSON* root, const std::string& topic, bool retain) {
    auto json = cJSON_PrintUnformatted(root);

    ESP_ASSERT_CHECK(esp_mqtt_client_publish(_client, topic.c_str(), json, 0, QOS_MIN_ONE, retain) >= 0);

    cJSON_free(json);
}

void MQTTConnection::publish_button_discovery(MQTTDiscovery metadata, std::function<void()> command_func) {
    publish_discovery("button", metadata, [this, command_func](auto json, auto object_id) {
        cJSON_AddStringToObject(json, "command_topic", (_topic_prefix + "set/" + object_id).c_str());
        cJSON_AddStringToObject(json, "payload_press", "true");

        register_callback(object_id, [command_func](auto data) {
            if (data == "true") {
                command_func();
            } else {
                ESP_LOGW(TAG, "Invalid button press payload '%s'", data);
            }
        });
    });
}

void MQTTConnection::publish_sensor_discovery(MQTTDiscovery metadata, MQTTSensorDiscovery component_metadata) {
    publish_discovery("sensor", metadata, [this, &component_metadata](auto json, auto object_id) {
        cJSON_AddStringToObject(json, "state_class", component_metadata.state_class);
        cJSON_AddStringToObject(json, "state_topic", (_topic_prefix + "state").c_str());
        cJSON_AddStringToObject(json, "unit_of_measurement", component_metadata.unit_of_measurement);
        cJSON_AddStringToObject(json, "value_template", component_metadata.value_template);
    });
}

void MQTTConnection::publish_switch_discovery(MQTTDiscovery metadata, MQTTSwitchDiscovery component_metadata,
                                              std::function<void(bool)> command_func) {
    publish_discovery("switch", metadata, [this, &component_metadata, command_func](auto json, auto object_id) {
        cJSON_AddStringToObject(json, "command_topic", (_topic_prefix + "set/" + object_id).c_str());
        cJSON_AddStringToObject(json, "payload_on", "on");
        cJSON_AddStringToObject(json, "payload_off", "off");
        cJSON_AddStringToObject(json, "state_topic", (_topic_prefix + "state").c_str());
        cJSON_AddStringToObject(json, "value_template", component_metadata.value_template);

        register_callback(object_id, [command_func](auto data) {
            if (data == "on") {
                command_func(true);
            } else if (data == "off") {
                command_func(false);
            } else {
                ESP_LOGW(TAG, "Cannot parse switch state '%s'", data);
            }
        });
    });
}

void MQTTConnection::publish_binary_sensor_discovery(MQTTDiscovery metadata,
                                                     MQTTBinarySensorDiscovery component_metadata) {
    publish_discovery("binary_sensor", metadata, [this, &component_metadata](auto json, auto object_id) {
        cJSON_AddBoolToObject(json, "payload_on", true);
        cJSON_AddBoolToObject(json, "payload_off", false);
        cJSON_AddStringToObject(json, "state_topic", (_topic_prefix + "state").c_str());
        cJSON_AddStringToObject(json, "value_template", component_metadata.value_template);
    });
}

void MQTTConnection::publish_number_discovery(MQTTDiscovery metadata, MQTTNumberDiscovery component_metadata,
                                              std::function<void(const std::string&)> command_func) {
    publish_discovery("binary_sensor", metadata, [this, &component_metadata, command_func](auto json, auto object_id) {
        if (component_metadata.unit_of_measurement) {
            cJSON_AddStringToObject(json, "unit_of_measurement", component_metadata.unit_of_measurement);
        }
        cJSON_AddNumberToObject(json, "min", component_metadata.min);
        cJSON_AddNumberToObject(json, "max", component_metadata.max);
        cJSON_AddNumberToObject(json, "step", component_metadata.step);
        cJSON_AddStringToObject(json, "command_topic", (_topic_prefix + "set/" + object_id).c_str());
        cJSON_AddStringToObject(json, "state_topic", (_topic_prefix + "state").c_str());
        cJSON_AddStringToObject(json, "value_template", component_metadata.value_template);

        register_callback(object_id, command_func);
    });
}

void MQTTConnection::publish_discovery(const char* component, const MQTTDiscovery& metadata,
                                       std::function<void(cJSON* json, const char* object_id)> func) {
    // Device classes can be found here: https://www.home-assistant.io/integrations/sensor/#device-class.
    // Entity category is either config or diagnostic.
    // MDI icons can be found here: https://pictogrammers.com/library/mdi/.

    const auto root = cJSON_CreateObject();

    cJSON_AddStringToObject(root, "name", metadata.name);
    cJSON_AddStringToObject(root, "icon", metadata.icon);
    if (metadata.entity_category) {
        cJSON_AddStringToObject(root, "entity_category", metadata.entity_category);
    }
    if (metadata.device_class) {
        cJSON_AddStringToObject(root, "device_class", metadata.device_class);
    }

    const auto availability = cJSON_AddArrayToObject(root, "availability");

    const auto availability_item = cJSON_CreateObject();
    cJSON_AddItemToArray(availability, availability_item);

    cJSON_AddStringToObject(availability_item, "topic",
                            strformat(CONFIG_MQTT_TOPIC_PREFIX "/%s/state", _device_id).c_str());
    cJSON_AddStringToObject(availability_item, "value_template", "{{ value_json.online }}");
    cJSON_AddBoolToObject(availability_item, "payload_available", true);

    cJSON_AddStringToObject(root, "availability_mode", "all");

    const auto device = cJSON_AddObjectToObject(root, "device");

    auto device_identifier = strformat("%s_%s", CONFIG_MQTT_TOPIC_PREFIX, _device_id);
    if (metadata.subdevice_id) {
        cJSON_AddStringToObject(device, "via_device", device_identifier.c_str());

        device_identifier += strformat("_%s", metadata.subdevice_id);
    }

    const auto identifiers = cJSON_AddArrayToObject(device, "identifiers");
    cJSON_AddItemToArray(identifiers, cJSON_CreateString(device_identifier.c_str()));

    cJSON_AddStringToObject(device, "manufacturer", CONFIG_MQTT_DEVICE_MANUFACTURER);
    cJSON_AddStringToObject(device, "model", CONFIG_MQTT_DEVICE_MODEL);
    cJSON_AddStringToObject(device, "model_id", CONFIG_MQTT_DEVICE_MODEL_ID);
    if (metadata.subdevice_name) {
        cJSON_AddStringToObject(device, "name", metadata.subdevice_name);
    } else {
        cJSON_AddStringToObject(device, "name", _configuration.device_name.c_str());
    }
    cJSON_AddStringToObject(device, "sw_version", get_firmware_version().c_str());

    const auto object_id = metadata.subdevice_id ? strformat("%s_%s", metadata.subdevice_id, metadata.object_id)
                                                 : std::string(metadata.object_id);

    cJSON_AddStringToObject(root, "unique_id",
                            strformat("%s_%s_%s", _device_id, component, object_id).c_str());
    cJSON_AddStringToObject(root, "object_id",
                            strformat("%s_%s", _configuration.device_entity_id, object_id).c_str());

    if (!metadata.enabled_by_default) {
        cJSON_AddBoolToObject(root, "enabled_by_default", false);
    }

    func(root, object_id.c_str());

    auto topic = strformat("homeassistant/%s/%s/%s/config", component, _device_id, object_id);
    _published_discovery_topics.insert(topic);
    publish_json(root, topic, true);

    cJSON_free(root);
}

void MQTTConnection::register_callback(const char* object_id, std::function<void(const std::string&)> callback) {
    _command_callbacks[object_id] = std::move(callback);
}

bool MQTTConnection::handle_discovery_prune(const std::string& topic, bool empty_message) {
    if (!topic.starts_with("homeassistant/")) {
        return false;
    }

    if (!empty_message && _published_discovery_topics.find(topic) == _published_discovery_topics.end()) {
        ESP_LOGI(TAG, "Pruning stale discovery topic %s", topic.c_str());
        ESP_ASSERT_CHECK(esp_mqtt_client_publish(_client, topic.c_str(), "", 0, QOS_MIN_ONE, true) >= 0);
    }

    return true;
}

std::string MQTTConnection::get_firmware_version() {
    const auto running_partition = esp_ota_get_running_partition();

    esp_app_desc_t running_app_info;
    ESP_ERROR_CHECK(esp_ota_get_partition_description(running_partition, &running_app_info));

    return running_app_info.version;
}

void MQTTConnection::send_state() {
    auto data = cJSON_CreateObject();
    send_state(data);
    cJSON_Delete(data);
}

void MQTTConnection::send_state(cJSON* data) {
    ESP_LOGI(TAG, "Publishing new state");

    ESP_ASSERT_CHECK(_client);

    cJSON_AddBoolToObject(data, "online", true);

    auto json = cJSON_PrintUnformatted(data);

    auto topic = _topic_prefix + "state";
    auto result = esp_mqtt_client_publish(_client, topic.c_str(), json, 0, QOS_MIN_ONE, true);
    if (result < 0) {
        ESP_LOGE(TAG, "Sending status update message failed with error %d", result);
    }

    cJSON_free(json);
}
