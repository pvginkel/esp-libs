#include "support.h"

#include "http_support.h"

LOG_TAG(http_support);

esp_err_t esp_http_get_response(esp_http_client_handle_t client, std::string& target, size_t max_length) {
    target.clear();

    constexpr size_t BUFFER_SIZE = 1024;
    const auto bufferSize = max_length > 0 ? std::min(max_length + 1, BUFFER_SIZE) : BUFFER_SIZE;

    auto buffer = std::make_unique<char[]>(bufferSize);

    while (true) {
        auto read = esp_http_client_read(client, buffer.get(), bufferSize);
        if (read < 0) {
            ESP_ERROR_RETURN(-read);
        }
        if (read == 0) {
            break;
        }

        if (max_length > 0 && target.length() + read > max_length) {
            ESP_ERROR_RETURN(ESP_ERR_INVALID_SIZE);
        }

        target.append(buffer.get(), read);
    }

    return ESP_OK;
}

esp_err_t esp_http_get_json(esp_http_client_handle_t client, cJSON*& data, size_t max_length) {
    std::string json;
    ESP_ERROR_RETURN(esp_http_get_response(client, json, max_length));

    data = cJSON_Parse(json.c_str());
    ESP_RETURN_ON_FALSE(data, ESP_ERR_INVALID_ARG, TAG, "Failed to parse JSON");

    return ESP_OK;
}
