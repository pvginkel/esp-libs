#include "support.h"

esp_err_t esp_http_upload_string(const esp_http_client_config_t& config, const char* const data) {
    auto err = ESP_OK;

    auto client = esp_http_client_init(&config);

    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_post_field(client, data, strlen(data));

    err = esp_http_client_perform(client);

    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    return err;
}

int hextoi(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'A' && c <= 'F') {
        return 10 + (c - 'A');
    }
    if (c >= 'a' && c <= 'f') {
        return 10 + (c - 'a');
    }
    return -1;
}
