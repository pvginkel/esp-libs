#pragma once

#include <string>

#include "cJSON.h"
#include "esp_http_client.h"

esp_err_t esp_http_get_response(esp_http_client_handle_t client, std::string& target, size_t max_length);
esp_err_t esp_http_get_json(esp_http_client_handle_t client, cJSON*& data, size_t max_length);
