#include "support.h"

#include "OTAManager.h"

#include "esp_app_format.h"
#include "esp_log.h"
#include "sdkconfig.h"

#define HASH_LENGTH 32  // SHA-256 hash length
#define BUFFER_SIZE 1024

LOG_TAG(OTAManager);

bool OTAManager::install_update(const std::string& firmware_url, const std::string& authorization) {
    auto update_partition = esp_ota_get_next_update_partition(nullptr);
    ESP_ASSERT_CHECK(update_partition);
    auto running_partition = esp_ota_get_running_partition();
    ESP_ASSERT_CHECK(running_partition);

    OTAConfig ota_config = {
        .endpoint = firmware_url.c_str(),
        .authorization = authorization.c_str(),
        .update_partition = update_partition,
        .running_partition = running_partition,
        .check_only = false,
    };

    bool firmware_installed = false;
    ESP_ERROR_CHECK(install_firmware(ota_config, firmware_installed));
    if (firmware_installed) {
        ESP_LOGI(TAG, "App update installed; restarting");

        ESP_ERROR_CHECK(esp_ota_set_boot_partition(ota_config.update_partition));

        return true;
    }

    return false;
}

esp_err_t OTAManager::install_firmware(OTAConfig& ota_config, bool& firmware_installed) {
    auto ota_busy = false;
    esp_ota_handle_t update_handle = 0;

    DEFER({
        if (ota_busy) {
            esp_ota_abort(update_handle);
        }
    });

    auto buffer = std::make_unique<char[]>(BUFFER_SIZE);
    auto firmware_size = 0;

    esp_http_client_config_t config = {
        .url = ota_config.endpoint,
        .timeout_ms = CONFIG_NETWORK_RECEIVE_TIMEOUT,
    };

    ESP_LOGI(TAG, "Getting firmware from '%s'", config.url);

    auto client = esp_http_client_init(&config);
    ESP_RETURN_ON_FALSE(client, ESP_FAIL, TAG, "Failed to init HTTP client");
    DEFER(esp_http_client_cleanup(client));

    ESP_ERROR_RETURN(esp_http_client_set_header(client, "Authorization", ota_config.authorization));

    ESP_ERROR_RETURN(esp_http_client_open(client, 0));

    auto length = esp_http_client_fetch_headers(client);
    if (length < 0) {
        return (esp_err_t)-length;
    }

    while (true) {
        auto read = esp_http_client_read(client, buffer.get(), BUFFER_SIZE);
        if (read < 0) {
            ESP_ERROR_RETURN(-read);
        }
        if (read == 0) {
            if (esp_http_client_is_complete_data_received(client)) {
                ESP_LOGI(TAG, "Connection closed");
                break;
            } else {
                ESP_RETURN_ON_ERROR(ESP_FAIL, TAG, "Stream not completely read");
            }
        }

        // If this is the first block we've read, parse the header.
        if (firmware_size == 0) {
            ESP_RETURN_ON_FALSE(
                read >= sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t) + sizeof(esp_app_desc_t),
                ESP_FAIL, TAG, "Did not receive enough data to parse the firmware header");

            // check current version with downloading
            esp_app_desc_t new_app_info;
            memcpy(&new_app_info, &buffer.get()[sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t)],
                   sizeof(esp_app_desc_t));

            esp_app_desc_t running_app_info;
            auto err = esp_ota_get_partition_description(ota_config.running_partition, &running_app_info);

            if (err == ESP_OK && !ota_config.force) {
                ESP_LOGI(TAG, "New firmware version: %s, current %s", new_app_info.version, running_app_info.version);

                if (strcmp(new_app_info.version, running_app_info.version) == 0) {
                    ESP_LOGI(TAG, "Firmware already up to date.");
                    return ESP_OK;
                }
            } else {
                ESP_LOGI(TAG, "New firmware version: %s", new_app_info.version);
            }

            if (ota_config.check_only) {
                firmware_installed = true;
                return ESP_OK;
            }

            auto last_invalid_app = esp_ota_get_last_invalid_partition();

            if (last_invalid_app != nullptr) {
                esp_app_desc_t invalid_app_info;
                ESP_ERROR_RETURN(esp_ota_get_partition_description(last_invalid_app, &invalid_app_info));

                ESP_LOGI(TAG, "Last invalid firmware version: %s", invalid_app_info.version);

                // Check current version with last invalid partition.
                if (strcmp(invalid_app_info.version, new_app_info.version) == 0 && !ota_config.force) {
                    ESP_LOGW(TAG, "Refusing to update to invalid firmware version.");
                    return ESP_OK;
                }
            }

            // esp_ota_begin checks whether the partition is an OTA partition.
            // We also want to be able to update the factory partition, hence this
            // work around.

            const auto update_partition_subtype = ota_config.update_partition->subtype;
            if (ota_config.update_partition->subtype == ESP_PARTITION_SUBTYPE_APP_FACTORY) {
                ((esp_partition_t*)ota_config.update_partition)->subtype = ESP_PARTITION_SUBTYPE_APP_OTA_0;
            }

            err = esp_ota_begin(ota_config.update_partition, OTA_WITH_SEQUENTIAL_WRITES, &update_handle);

            ((esp_partition_t*)ota_config.update_partition)->subtype = update_partition_subtype;

            ESP_ERROR_RETURN(err);

            ota_busy = true;

            ESP_LOGI(TAG, "Downloading new firmware");
        }

        ESP_ERROR_RETURN(esp_ota_write(update_handle, (const void*)buffer.get(), read));

        firmware_size += read;

        ESP_LOGD(TAG, "Written %d bytes, total %d", read, firmware_size);
    }

    ESP_RETURN_ON_FALSE(esp_http_client_is_complete_data_received(client), ESP_FAIL, TAG, "Stream not fully read");

    ESP_ERROR_RETURN(esp_ota_end(update_handle));

    ota_busy = false;
    firmware_installed = true;

    return ESP_OK;
}

bool OTAManager::parse_hash(char* buffer, uint8_t* hash) {
    for (auto i = 0; i < HASH_LENGTH; i++) {
        auto h = hextoi(buffer[i * 2]);
        auto l = hextoi(buffer[i * 2 + 1]);

        if (h == -1 || l == -1) {
            return false;
        }

        hash[i] = uint8_t(h << 4 | l);
    }

    return true;
}
