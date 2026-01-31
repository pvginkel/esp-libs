#include "support.h"

#include "LogManager.h"

#include "cJSON.h"
#include "esp_http_client.h"
#include "esp_wifi.h"

constexpr auto BUFFER_SIZE = 1024;
constexpr auto BLOCK_SIZE = 10;
constexpr auto MAX_MESSAGES = 100;

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

    return _instance->_mutex.with<int>([message, va]() {
        while (_instance->_messages.size() > MAX_MESSAGES) {
            free(_instance->_messages[0].buffer);
            _instance->_messages.erase(_instance->_messages.begin());
        }

        auto result = vsnprintf(_buffer, BUFFER_SIZE, message, va);

        bool startTimer = false;

        if (result >= 0 && result < BUFFER_SIZE) {
            auto buffer_copy = strdup(_buffer);
            ESP_ASSERT_CHECK(buffer_copy);

            startTimer = !_instance->_device_entity_id.empty() && _instance->_messages.size() == 0;
            _instance->_messages.push_back(Message(buffer_copy, esp_get_millis()));
        }

        if (startTimer) {
            _instance->start_timer();
        }

        return result;
    });
}

LogManager::LogManager() { _instance = this; }

esp_err_t LogManager::begin(const std::string& logging_url) {
    _logging_url = logging_url;
    _default_log_handler = esp_log_set_vprintf(log_handler);

    const esp_timer_create_args_t displayOffTimerArgs = {
        .callback = [](void* arg) { ((LogManager*)arg)->upload_logs(); },
        .arg = this,
        .name = "logManagerTimer",
    };

    // Timer is application-lifetime; no cleanup needed.
    ESP_ERROR_RETURN(esp_timer_create(&displayOffTimerArgs, &_log_timer));

    esp_register_shutdown_handler([]() {
        if (_instance) {
            ESP_LOGI(TAG, "Uploading log messages before restart");

            _instance->_shutting_down = true;
            _instance->upload_logs();
        }
    });

    return ESP_OK;
}

void LogManager::set_device_entity_id(const std::string& device_entity_id) {
    auto startTimer = _mutex.with<bool>([this, &device_entity_id]() {
        _device_entity_id = device_entity_id;

        return _messages.size() > 0;
    });

    if (startTimer) {
        this->start_timer();
    }
}

esp_err_t LogManager::upload_logs() {
    auto messages = _mutex.with<std::vector<Message>>([this]() {
        if (_device_entity_id.empty()) {
            return std::vector<Message>();
        }

        auto messages = _messages;

        _messages.clear();

        return messages;
    });

    std::string buffer;
    size_t offset = 0;

    while (offset < messages.size()) {
        buffer.clear();

        auto millis = esp_get_millis();

        for (auto i = 0; i < BLOCK_SIZE; i++) {
            auto index = offset++;
            if (index >= messages.size()) {
                break;
            }

            auto message = messages[index];

            auto root = cJSON_CreateObject();

            cJSON_AddStringToObject(root, "message", message.buffer);
            cJSON_AddNumberToObject(root, "relative_time", double(millis - message.time));
            cJSON_AddStringToObject(root, "entity_id", _device_entity_id.c_str());

            auto json = cJSON_PrintUnformatted(root);
            cJSON_Delete(root);

            buffer.append(json);
            buffer.append("\n");

            cJSON_free(json);

            free(message.buffer);
        }

        esp_http_client_config_t config = {
            .url = _logging_url.c_str(),
            .timeout_ms = CONFIG_NETWORK_RECEIVE_TIMEOUT,
            .crt_bundle_attach = esp_crt_bundle_attach,
        };

        auto client = esp_http_client_init(&config);
        ESP_RETURN_ON_FALSE(client, ESP_FAIL, TAG, "Failed to init HTTP client");
        DEFER(esp_http_client_cleanup(client));

        ESP_ERROR_RETURN(esp_http_client_set_method(client, HTTP_METHOD_POST));
        ESP_ERROR_RETURN(esp_http_client_set_post_field(client, buffer.c_str(), buffer.length()));
        ESP_ERROR_RETURN(esp_http_client_perform(client));
    }

    return ESP_OK;
}

void LogManager::start_timer() {
    if (!_instance->_shutting_down) {
        ESP_ERROR_CHECK(esp_timer_start_once(_log_timer, ESP_TIMER_MS(CONFIG_MDM_LOG_INTERVAL)));
    }
}
