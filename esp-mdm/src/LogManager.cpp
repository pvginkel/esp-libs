#include "support.h"

#include "LogManager.h"

#include "cJSON.h"

constexpr auto BUFFER_SIZE = 1024;
constexpr auto MAX_MESSAGES = 100;
constexpr auto SHUTDOWN_TIMEOUT_MS = 5000;
constexpr auto SHUTDOWN_POLL_INTERVAL_MS = 100;
constexpr auto RETRY_PUBLISH_INTERVAL_MS = 1000;

LOG_TAG(LogManager);

LogManager* LogManager::_instance = nullptr;
// Intentional one-time allocation; freeing on shutdown provides no benefit.
char* LogManager::_buffer = new char[BUFFER_SIZE];

int LogManager::log_handler(const char* message, va_list va) {
    if (!_instance) {
        return 0;
    }

    va_list vaCopy;
    va_copy(vaCopy, va);

    _instance->_default_log_handler(message, vaCopy);

    va_end(vaCopy);

    auto result = _instance->_mutex.with<int>([message, va]() {
        while (_instance->_messages.size() > MAX_MESSAGES) {
            free(_instance->_messages[0].buffer);
            _instance->_messages.erase(_instance->_messages.begin());
        }

        auto result = vsnprintf(_buffer, BUFFER_SIZE, message, va);

        if (result >= 0 && result < BUFFER_SIZE) {
            auto buffer_copy = strdup(_buffer);
            ESP_ASSERT_CHECK(buffer_copy);

            _instance->_messages.push_back(Message(buffer_copy));
        }

        return result;
    });

    _instance->_signal.signal();

    return result;
}

LogManager::LogManager(MQTTConnection& mqtt_connection) : _mqtt_connection(mqtt_connection) {
    ESP_ASSERT_CHECK(!_instance);

    _instance = this;
}

esp_err_t LogManager::begin() {
    _default_log_handler = esp_log_set_vprintf(log_handler);

    _mqtt_connection.on_connected_changed([this](auto state) {
        if (state.connected) {
            _signal.signal();
        }
    });

    xTaskCreatePinnedToCore([](void* arg) { ((LogManager*)arg)->task_loop(); }, "log_manager",
                            CONFIG_MDM_LOG_TASK_STACK_SIZE, this, 1, &_task_handle, CONFIG_MDM_LOG_TASK_CORE_ID);

    esp_register_shutdown_handler([]() {
        if (_instance) {
            ESP_LOGI(TAG, "Flushing log messages before restart");

            _instance->_shutting_down = true;
            _instance->_signal.signal();

            // Wait for messages to drain, polling every 100ms for up to 5 seconds.
            auto elapsed = 0;
            do {
                vTaskDelay(pdMS_TO_TICKS(SHUTDOWN_POLL_INTERVAL_MS));
                elapsed += SHUTDOWN_POLL_INTERVAL_MS;
            } while (elapsed < SHUTDOWN_TIMEOUT_MS && _instance->get_message_count() > 0);

            if (_instance->get_message_count() > 0) {
                ESP_LOGW(TAG, "Shutdown timeout; dropping remaining log messages");
            }
        }
    });

    return ESP_OK;
}

void LogManager::set_device_entity_id(const std::string& device_entity_id) {
    _mutex.with<void>([this, &device_entity_id]() { _device_entity_id = device_entity_id; });

    _signal.signal();
}

bool LogManager::can_send() {
    return _mqtt_connection.is_connected() && _mutex.with<bool>([this]() { return !_device_entity_id.empty(); });
}

size_t LogManager::get_message_count() {
    return _mutex.with<size_t>([this]() { return _messages.size(); });
}

void LogManager::task_loop() {
    while (true) {
        _signal.wait();

        publish_messages();
    }
}

void LogManager::publish_messages() {
    constexpr int MAX_BATCH_SIZE = 10;

    while (can_send()) {
        // Pop up to MAX_BATCH_SIZE messages from the queue.
        std::vector<char*> buffers;
        std::string entity_id;

        _mutex.with<void>([this, &buffers, &entity_id]() {
            entity_id = _device_entity_id;

            while (!_messages.empty() && buffers.size() < MAX_BATCH_SIZE) {
                buffers.push_back(_messages[0].buffer);
                _messages.erase(_messages.begin());
            }
        });

        if (buffers.empty()) {
            break;
        }

        // Build NDJSON payload.
        std::string payload;
        for (auto buffer : buffers) {
            auto root = cJSON_CreateObject();
            cJSON_AddStringToObject(root, "message", buffer);
            cJSON_AddStringToObject(root, "entity_id", entity_id.c_str());

            auto json = cJSON_PrintUnformatted(root);
            cJSON_Delete(root);

            payload += json;
            payload += '\n';

            cJSON_free(json);
        }

        while (true) {
            auto success = _mqtt_connection.publish(CONFIG_MDM_LOG_TOPIC, payload, 1, false);
            if (success) {
                break;
            }

            // We retry indefinitely. There is no point in doing anything else. At
            // some point either we will be able to get these messages out, or, we'll
            // restart.
            vTaskDelay(pdMS_TO_TICKS(RETRY_PUBLISH_INTERVAL_MS));
        }

        for (auto buffer : buffers) {
            free(buffer);
        }

        // Yield to allow MQTT client to process ACKs.
        taskYIELD();
    }
}
