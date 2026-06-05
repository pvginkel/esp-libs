#include "support.h"

#include "MQTTConnection.h"

#include <charconv>
#include <cstdint>

#include "MQTTSupport.h"
#include "defer.h"
#include "esp_mac.h"
#include "esp_ota_ops.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

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
            // The transport is up: allow QoS>0 publishes through back-pressure.
            set_transport_connected(true);
            // On connect we're publishing a large number of messages for metadata.
            // We need to do this outside of the MQTT loop because otherwise we
            // wouldn't be able to process in flight ACKs.
            _queue->enqueue([this]() { handle_connected(); });
            break;

        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT disconnected");

            _connected = false;
            // Release any producers blocked on the in-flight budget; the ACKs they
            // were waiting for will never arrive on this connection.
            set_transport_connected(false);
            _connected_changed.queue(_queue, {false});
            break;

        case MQTT_EVENT_SUBSCRIBED: {
            const auto error_type = event->error_handle ? (int)event->error_handle->error_type : 0;
            if (error_type) {
                ESP_LOGI(TAG, "MQTT subscribed error %d", error_type);
            } else {
                ESP_LOGI(TAG, "MQTT subscribed");
            }

            auto callback_it = _subscribed_callbacks.find(event->msg_id);
            if (callback_it != _subscribed_callbacks.end()) {
                auto cb = std::move(callback_it->second);
                _subscribed_callbacks.erase(callback_it);

                if (error_type == 0) {
                    cb();
                }
            }
            break;
        }

        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI(TAG, "MQTT unsubscribed");
            break;

        case MQTT_EVENT_PUBLISHED:
            // A QoS>0 publish was acknowledged (PUBACK/PUBCOMP). Free its in-flight
            // slot and wake a waiting producer. This is the consumer side of the
            // back-pressure; it only signals and never blocks.
            release_inflight_id(event->msg_id);
            break;

        case MQTT_EVENT_DELETED:
            // esp-mqtt expired this message from its outbox before it could be
            // acknowledged (requires CONFIG_MQTT_REPORT_DELETED_MESSAGES). Free its
            // slot so an undeliverable message doesn't hold back-pressure forever.
            ESP_LOGD(TAG, "MQTT message %d deleted from outbox", event->msg_id);
            release_inflight_id(event->msg_id);
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
    subscribe(_topic_prefix + "set/#", [this]() {
        _connected = true;
        _connected_changed.call({true});
    });

    publish_configuration();

    _published_discovery_topics.clear();
    _publish_discovery.call();

    auto discovery_topic = strformat("homeassistant/+/%s/+/config", _device_id);
    subscribe(discovery_topic);
    _queue->enqueue_delayed([this, discovery_topic]() { unsubscribe(discovery_topic); }, 60000);
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

    auto data = event->data_len ? std::string(event->data, event->data_len) : std::string();

    _queue->enqueue([this, topic, data]() {
        // Check for custom topic subscriptions.
        auto topic_it = _topic_callbacks.find(topic);
        if (topic_it != _topic_callbacks.end()) {
            topic_it->second(data);
            return;
        }

        if (!topic.starts_with(_topic_prefix)) {
            ESP_LOGE(TAG, "Unexpected topic %s topic len %d data len %d", topic.c_str(), topic.length(), data.length());
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
            it->second(data);
        } else {
            ESP_LOGW(TAG, "No callback registered for object_id '%s'", object_id.c_str());
        }
    });
}

int MQTTConnection::subscribe(const std::string& topic) {
    ESP_LOGI(TAG, "Subscribing to topic %s", topic.c_str());

    const auto ret = esp_mqtt_client_subscribe(_client, topic.c_str(), 0);
    ESP_ASSERT_CHECK(ret >= 0);

    return ret;
}

void MQTTConnection::subscribe(const std::string& topic, std::function<void()> subscribed_func) {
    const auto ret = subscribe(topic);

    _subscribed_callbacks[ret] = subscribed_func;
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
    ESP_ASSERT_CHECK(root);
    DEFER(cJSON_Delete(root));

    cJSON_AddStringToObject(root, "unique_id", uniqueIdentifier.c_str());

    auto device = cJSON_AddObjectToObject(root, "device");
    cJSON_AddStringToObject(device, "manufacturer", _configuration.device.manufacturer.c_str());
    cJSON_AddStringToObject(device, "model", _configuration.device.model.c_str());
    cJSON_AddStringToObject(device, "name", _configuration.device_name.c_str());
    cJSON_AddStringToObject(device, "firmware_version", get_firmware_version().c_str());

    _create_configuration.call(root);

    publish_json(root, _topic_prefix + "configuration", true);
}

void MQTTConnection::publish_json(cJSON* root, const std::string& topic, bool retain) {
    auto json = cJSON_PrintUnformatted(root);

    publish_with_backpressure(topic.c_str(), json, 0, QOS_MIN_ONE, retain);

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
    publish_discovery("sensor", metadata, [this, component_metadata](auto json, auto object_id) {
        cJSON_AddStringToObject(json, "state_class", component_metadata.state_class);
        cJSON_AddStringToObject(json, "state_topic", (_topic_prefix + "state").c_str());
        cJSON_AddStringToObject(json, "unit_of_measurement", component_metadata.unit_of_measurement);
        cJSON_AddStringToObject(json, "value_template", component_metadata.value_template);
    });
}

void MQTTConnection::publish_switch_discovery(MQTTDiscovery metadata, MQTTSwitchDiscovery component_metadata,
                                              std::function<void(bool)> command_func) {
    publish_discovery("switch", metadata, [this, component_metadata, command_func](auto json, auto object_id) {
        cJSON_AddStringToObject(json, "command_topic", (_topic_prefix + "set/" + object_id).c_str());
        cJSON_AddStringToObject(json, "payload_on", "on");
        cJSON_AddStringToObject(json, "payload_off", "off");
        cJSON_AddStringToObject(json, "state_topic", (_topic_prefix + "state").c_str());
        cJSON_AddStringToObject(json, "value_template", component_metadata.value_template);

        register_callback(object_id, [command_func](auto data) {
            const auto state = parse_switch_state(data.c_str());
            if (state == SwitchState::ON) {
                command_func(true);
            } else if (state == SwitchState::OFF) {
                command_func(false);
            }
        });
    });
}

void MQTTConnection::publish_binary_sensor_discovery(MQTTDiscovery metadata,
                                                     MQTTBinarySensorDiscovery component_metadata) {
    publish_discovery("binary_sensor", metadata, [this, component_metadata](auto json, auto object_id) {
        cJSON_AddBoolToObject(json, "payload_on", true);
        cJSON_AddBoolToObject(json, "payload_off", false);
        cJSON_AddStringToObject(json, "state_topic", (_topic_prefix + "state").c_str());
        cJSON_AddStringToObject(json, "value_template", component_metadata.value_template);
    });
}

void MQTTConnection::publish_number_discovery(MQTTDiscovery metadata, MQTTNumberDiscovery component_metadata,
                                              std::function<void(const std::string&)> command_func) {
    publish_discovery("number", metadata, [this, component_metadata, command_func](auto json, auto object_id) {
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

void MQTTConnection::publish_device_automation(MQTTDeviceAutomationDiscovery metadata) {
    // Device classes can be found here:
    // https://www.home-assistant.io/integrations/device_trigger.mqtt/

    const auto root = cJSON_CreateObject();
    ESP_ASSERT_CHECK(root);
    DEFER(cJSON_Delete(root));

    cJSON_AddStringToObject(root, "automation_type", "trigger");
    cJSON_AddStringToObject(root, "payload", metadata.trigger_value);
    cJSON_AddStringToObject(root, "subtype", metadata.trigger_value);
    cJSON_AddStringToObject(root, "topic", (_topic_prefix + metadata.trigger_name).c_str());
    cJSON_AddStringToObject(root, "type", metadata.trigger_name);

    add_device_metadata(root, metadata.subdevice_id, metadata.subdevice_name);

    auto topic = strformat("homeassistant/device_automation/%s/%s_%s/config", _device_id, metadata.trigger_name,
                           metadata.trigger_value);
    _published_discovery_topics.insert(topic);
    publish_json(root, topic, true);
}

void MQTTConnection::publish_discovery(const char* component, const MQTTDiscovery& metadata,
                                       std::function<void(cJSON* json, const char* object_id)> func) {
    // Device classes can be found here: https://www.home-assistant.io/integrations/sensor/#device-class.
    // Entity category is either config or diagnostic.
    // MDI icons can be found here: https://pictogrammers.com/library/mdi/.

    const auto root = cJSON_CreateObject();
    ESP_ASSERT_CHECK(root);
    DEFER(cJSON_Delete(root));

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

    add_device_metadata(root, metadata.subdevice_id, metadata.subdevice_name);

    const auto object_id = metadata.subdevice_id ? strformat("%s_%s", metadata.subdevice_id, metadata.object_id)
                                                 : std::string(metadata.object_id);

    cJSON_AddStringToObject(root, "unique_id", strformat("%s_%s_%s", _device_id, component, object_id).c_str());
    cJSON_AddStringToObject(root, "object_id", strformat("%s_%s", _configuration.device_entity_id, object_id).c_str());

    if (!metadata.enabled_by_default) {
        cJSON_AddBoolToObject(root, "enabled_by_default", false);
    }

    func(root, object_id.c_str());

    auto topic = strformat("homeassistant/%s/%s/%s/config", component, _device_id, object_id);
    _published_discovery_topics.insert(topic);
    publish_json(root, topic, true);
}

void MQTTConnection::add_device_metadata(cJSON* root, const char* subdevice_id, const char* subdevice_name) {
    const auto device = cJSON_AddObjectToObject(root, "device");

    auto device_identifier = strformat("%s_%s", CONFIG_MQTT_TOPIC_PREFIX, _device_id);
    if (subdevice_id) {
        cJSON_AddStringToObject(device, "via_device", device_identifier.c_str());

        device_identifier += strformat("_%s", subdevice_id);
    }

    const auto identifiers = cJSON_AddArrayToObject(device, "identifiers");
    cJSON_AddItemToArray(identifiers, cJSON_CreateString(device_identifier.c_str()));

    cJSON_AddStringToObject(device, "manufacturer", _configuration.device.manufacturer.c_str());
    cJSON_AddStringToObject(device, "model", _configuration.device.model.c_str());
    cJSON_AddStringToObject(device, "model_id", _configuration.device.model_id.c_str());
    if (subdevice_name) {
        cJSON_AddStringToObject(device, "name", subdevice_name);
    } else {
        cJSON_AddStringToObject(device, "name", _configuration.device_name.c_str());
    }
    cJSON_AddStringToObject(device, "sw_version", get_firmware_version().c_str());
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
        // This runs on the MQTT event task, which is the consumer that frees
        // in-flight slots. Publishing here could block on back-pressure and
        // deadlock the ACK draining, so hand the tombstone publish to the queue.
        _queue->enqueue([this, topic]() { publish_with_backpressure(topic.c_str(), "", 0, QOS_MIN_ONE, true); });
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
    publish_with_backpressure(topic.c_str(), json, 0, QOS_MIN_ONE, true);

    cJSON_free(json);
}

void MQTTConnection::send_trigger(const char* name, const char* value) {
    ESP_LOGI(TAG, "Publishing new action %s=%s", name, value);

    ESP_ASSERT_CHECK(_client);

    auto topic = _topic_prefix + name;
    publish_with_backpressure(topic.c_str(), value, 0, QOS_MIN_ONE, false);
}

bool MQTTConnection::publish(const std::string& topic, const std::string& payload, int qos, bool retain) {
    if (!_client) {
        ESP_LOGD(TAG, "Cannot publish, client not initialized");
        return false;
    }

    auto result = publish_with_backpressure(topic.c_str(), payload.c_str(), payload.length(), qos, retain);
    if (result < 0) {
        ESP_LOGD(TAG, "Publish tov%s failed with error %d", topic.c_str(), result);
        return false;
    }

    return true;
}

int MQTTConnection::publish_with_backpressure(const char* topic, const char* data, int len, int qos, bool retain) {
    // Back-pressure for QoS>0: keep the number of unacknowledged in-flight
    // publishes below the broker's MQTT5 Receive Maximum. This blocks the calling
    // (producer) task until a slot frees; the MQTT event task is the consumer
    // (it frees slots on MQTT_EVENT_PUBLISHED / MQTT_EVENT_DELETED), so it must
    // never reach this path - that would deadlock the very task that drains the
    // acknowledgements. QoS 0 is fire-and-forget and does not count against the
    // budget.
    if (qos > 0) {
        // Re-check period so a half-open connection (no DISCONNECTED yet) cannot
        // pin a producer indefinitely; it also bounds the disconnect wake-up.
        constexpr TickType_t MAX_SLOT_WAIT = pdMS_TO_TICKS(250);

        for (;;) {
            const auto now = esp_timer_get_time();
            bool connected;
            bool admitted = false;
            int purged;
            int64_t soonest_deadline = INT64_MAX;
            {
                auto lock = _inflight_mutex.take();
                connected = _transport_connected;
                purged = purge_expired_inflight(now);
                if (connected && (int)_inflight.size() + _reserved < CONFIG_MQTT_MAX_INFLIGHT_QOS) {
                    // Reserve the slot now; the msg_id is only known once
                    // esp_mqtt_client_publish returns, so we cannot insert the
                    // map entry yet without holding the lock across the publish.
                    _reserved++;
                    admitted = true;
                } else {
                    // Find when the oldest in-flight message will time out so we
                    // can wake to reclaim it even if no event ever arrives.
                    for (const auto& entry : _inflight) {
                        const int64_t deadline = entry.second + (int64_t)CONFIG_MQTT_INFLIGHT_TTL_MS * 1000;
                        if (deadline < soonest_deadline) {
                            soonest_deadline = deadline;
                        }
                    }
                }
            }

            if (purged > 0) {
                // A TTL expiry opened slots; wake another waiter to claim them.
                _slot_available.signal();
            }

            if (!connected) {
                // Don't queue into the outbox while offline; drop and let the
                // caller re-publish on reconnect. Chain-wake any other producers
                // blocked here so a disconnect releases them all promptly.
                _slot_available.signal();
                ESP_LOGD(TAG, "Publish to %s dropped, transport disconnected", topic);
                return -1;
            }

            if (admitted) {
                break;
            }

            TickType_t wait_ticks = MAX_SLOT_WAIT;
            if (soonest_deadline != INT64_MAX) {
                const int64_t remaining_us = soonest_deadline - esp_timer_get_time();
                if (remaining_us <= 0) {
                    // Already due; loop to purge it immediately.
                    continue;
                }
                TickType_t ttl_ticks = pdMS_TO_TICKS(remaining_us / 1000);
                if (ttl_ticks < 1) {
                    ttl_ticks = 1;
                }
                if (ttl_ticks < wait_ticks) {
                    wait_ticks = ttl_ticks;
                }
            }

            _slot_available.wait(wait_ticks);
        }
    }

    auto result = esp_mqtt_client_publish(_client, topic, data, len, qos, retain);

    if (qos > 0) {
        {
            auto lock = _inflight_mutex.take();
            if (_reserved > 0) {
                _reserved--;
            }
            // Convert the reservation into a tracked in-flight message. Track it
            // regardless of connection state: if it really went out it will be
            // acked, deleted, or TTL-reclaimed; a stale entry simply ages out.
            if (result > 0) {
                _inflight[result] = esp_timer_get_time();
            }
        }
        if (result < 0) {
            // Nothing was tracked, so the reserved slot is now free.
            _slot_available.signal();
        }
    }

    if (result < 0) {
        ESP_LOGW(TAG, "Publish to %s failed with error %d", topic, result);
    }

    return result;
}

void MQTTConnection::set_transport_connected(bool connected) {
    {
        auto lock = _inflight_mutex.take();
        _transport_connected = connected;
    }
    // Wake every producer: on connect so they can resume, on disconnect so they
    // observe the transport is down and fail fast instead of blocking on
    // acknowledgements that will never arrive. In-flight entries are intentionally
    // left in place - they are either resent (and acked) on reconnect or aged out
    // by their TTL, which keeps the budget honest across a flap.
    _slot_available.signal();
}

void MQTTConnection::release_inflight_id(int msg_id) {
    {
        auto lock = _inflight_mutex.take();
        // erase() of an untracked id (e.g. a QoS 0 deletion reported as id 0, or a
        // duplicate event) is a harmless no-op, which makes this idempotent.
        _inflight.erase(msg_id);
    }
    _slot_available.signal();
}

int MQTTConnection::purge_expired_inflight(int64_t now) {
    // Caller must hold _inflight_mutex. Reclaim slots for messages that have been
    // in flight longer than the TTL - esp-mqtt can drop a message from its outbox
    // without ever posting an event, which would otherwise leak the slot until the
    // next disconnect.
    const int64_t ttl_us = (int64_t)CONFIG_MQTT_INFLIGHT_TTL_MS * 1000;
    int purged = 0;
    for (auto it = _inflight.begin(); it != _inflight.end();) {
        if (now - it->second > ttl_us) {
            it = _inflight.erase(it);
            purged++;
        } else {
            ++it;
        }
    }
    return purged;
}
