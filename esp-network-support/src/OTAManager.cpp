#include "support.h"

#include "OTAManager.h"

#include "NetworkConfiguration.h"
#include "esp_app_format.h"
#include "esp_log.h"

constexpr auto OTA_INITIAL_CHECK_INTERVAL = 5;
constexpr auto HASH_LENGTH = 32;  // SHA-256 hash length
constexpr auto BUFFER_SIZE = 1024;

LOG_TAG(OTAManager);

OTAManager::OTAManager() : _update_timer(nullptr) {}

void OTAManager::begin() {
    const esp_timer_create_args_t displayOffTimerArgs = {
        .callback = [](void *arg) { ((OTAManager *)arg)->update_check(); },
        .arg = this,
        .name = "updateTimer",
    };

    ESP_ERROR_CHECK(esp_timer_create(&displayOffTimerArgs, &_update_timer));
    ESP_ERROR_CHECK(esp_timer_start_once(_update_timer, ESP_TIMER_SECONDS(OTA_INITIAL_CHECK_INTERVAL)));
    ESP_LOGI(TAG, "Started OTA timer");
}

void OTAManager::bootstrap() {
    const auto ota_endpoint = get_ota_endpoint();
    ESP_ERROR_ASSERT(ota_endpoint);

    auto ota_partition = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, "ota");
    ESP_ERROR_ASSERT(ota_partition);
    auto factory_partition =
        esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_FACTORY, "factory");
    ESP_ERROR_ASSERT(factory_partition);

    OTAConfig ota_config = {
        .endpoint = ota_endpoint.value().c_str(),
        .update_partition = ota_partition,
        .running_partition = ota_partition,
        .check_only = false,
        .force = false,
    };

    const auto installed = install_firmware(ota_config);

    auto err = esp_ota_set_boot_partition(ota_config.update_partition);

    if (!installed && err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to set boot partition; forcing new firmware");

        ota_config.force = true;

        install_firmware(ota_config);

        err = esp_ota_set_boot_partition(ota_config.update_partition);
    }

    ESP_ERROR_CHECK(err);

    esp_restart();
}

void OTAManager::update_check() {
    if (install_update()) {
        ESP_LOGI(TAG, "Firmware installed successfully; restarting system");

        esp_restart();
        return;
    }

    ESP_ERROR_CHECK(esp_timer_start_once(_update_timer, ESP_TIMER_SECONDS(CONFIG_OTA_CHECK_INTERVAL)));
}

bool OTAManager::install_update() {
    const auto ota_endpoint = get_ota_endpoint();
    if (!ota_endpoint) {
        return false;
    }

    auto ota_partition = esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, "ota");
    if (ota_partition) {
        auto factory_partition =
            esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_FACTORY, "factory");
        ESP_ERROR_ASSERT(factory_partition);

        ESP_LOGI(TAG, "Detected single partition mode; checking bootstrapper firmware");

        OTAConfig config = {
            .endpoint = CONFIG_OTA_BOOT_ENDPOINT,
            .update_partition = factory_partition,
            .running_partition = factory_partition,
            .check_only = false,
        };

        if (install_firmware(config)) {
            ESP_LOGI(TAG, "Bootstrapper firmware updated; restarting");

            ESP_ERROR_CHECK(esp_ota_set_boot_partition(factory_partition));

            return true;
        }

        config = {
            .endpoint = ota_endpoint.value().c_str(),
            .update_partition = ota_partition,
            .running_partition = ota_partition,
            .check_only = true,
        };

        if (install_firmware(config)) {
            ESP_LOGI(TAG, "App update found; restarting to install update");

            ESP_ERROR_CHECK(esp_ota_set_boot_partition(factory_partition));

            return true;
        }

        return false;
    } else {
        auto update_partition = esp_ota_get_next_update_partition(nullptr);
        ESP_ERROR_ASSERT(update_partition);
        auto running_partition = esp_ota_get_running_partition();
        ESP_ERROR_ASSERT(running_partition);

        OTAConfig ota_config = {
            .endpoint = ota_endpoint.value().c_str(),
            .update_partition = update_partition,
            .running_partition = running_partition,
            .check_only = false,
        };

        if (install_firmware(ota_config)) {
            ESP_LOGI(TAG, "App update installed; restarting");

            ESP_ERROR_CHECK(esp_ota_set_boot_partition(ota_config.update_partition));

            return true;
        }

        return false;
    }
}

bool OTAManager::install_firmware(OTAConfig &ota_config) {
    auto firmware_installed = false;
    auto ota_busy = false;

    auto buffer = new char[BUFFER_SIZE];
    auto firmware_size = 0;
    esp_ota_handle_t update_handle = 0;

    esp_http_client_config_t config = {
        .url = ota_config.endpoint,
        .timeout_ms = CONFIG_OTA_RECV_TIMEOUT,
    };

    ESP_LOGI(TAG, "Getting firmware from '%s'", config.url);

    auto client = esp_http_client_init(&config);

    ESP_ERROR_CHECK_JUMP(esp_http_client_open(client, 0), end);

    esp_http_client_fetch_headers(client);

    while (true) {
        auto read = esp_http_client_read(client, buffer, BUFFER_SIZE);

        if (read < 0) {
            ESP_LOGE(TAG, "Error while reading from HTTP stream");
            goto end;
        }

        if (read == 0) {
            // As esp_http_client_read never returns negative error code, we rely on
            // `errno` to check for underlying transport connectivity closure if any.

            if (errno == ECONNRESET || errno == ENOTCONN) {
                ESP_LOGE(TAG, "Connection closed unexpectedly, errno = %d", errno);
                goto end;
            } else if (esp_http_client_is_complete_data_received(client)) {
                ESP_LOGI(TAG, "Connection closed");
                break;
            } else {
                ESP_LOGE(TAG, "Stream not completely read");
                goto end;
            }
        }

        // If this is the first block we've read, parse the header.
        if (firmware_size == 0) {
            if (read < sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t) + sizeof(esp_app_desc_t)) {
                ESP_LOGE(TAG, "Did not receive enough data to parse the firmware header");
                goto end;
            }

            // check current version with downloading
            esp_app_desc_t new_app_info;
            memcpy(&new_app_info, &buffer[sizeof(esp_image_header_t) + sizeof(esp_image_segment_header_t)],
                   sizeof(esp_app_desc_t));

            esp_app_desc_t running_app_info;
            auto err = esp_ota_get_partition_description(ota_config.running_partition, &running_app_info);

            if (err == ESP_OK && !ota_config.force) {
                ESP_LOGI(TAG, "New firmware version: %s, current %s", new_app_info.version, running_app_info.version);

                if (strcmp(new_app_info.version, running_app_info.version) == 0) {
                    ESP_LOGI(TAG, "Firmware already up to date.");
                    goto end;
                }
            } else {
                ESP_LOGI(TAG, "New firmware version: %s", new_app_info.version);
            }

            if (ota_config.check_only) {
                firmware_installed = true;

                goto end;
            }

            auto last_invalid_app = esp_ota_get_last_invalid_partition();

            if (last_invalid_app != nullptr) {
                esp_app_desc_t invalid_app_info;
                ESP_ERROR_CHECK_JUMP(esp_ota_get_partition_description(last_invalid_app, &invalid_app_info), end);

                ESP_LOGI(TAG, "Last invalid firmware version: %s", invalid_app_info.version);

                // Check current version with last invalid partition.
                if (strcmp(invalid_app_info.version, new_app_info.version) == 0 && !ota_config.force) {
                    ESP_LOGW(TAG, "Refusing to update to invalid firmware version.");
                    goto end;
                }
            }

            // esp_ota_begin checks whether the partition is an OTA partition.
            // We also want to be able to update the factory partition, hence this
            // work around.

            const auto update_partition_subtype = ota_config.update_partition->subtype;
            if (ota_config.update_partition->subtype == ESP_PARTITION_SUBTYPE_APP_FACTORY) {
                ((esp_partition_t *)ota_config.update_partition)->subtype = ESP_PARTITION_SUBTYPE_APP_OTA_0;
            }

            err = esp_ota_begin(ota_config.update_partition, OTA_WITH_SEQUENTIAL_WRITES, &update_handle);

            ((esp_partition_t *)ota_config.update_partition)->subtype = update_partition_subtype;

            ESP_ERROR_CHECK_JUMP(err, end);

            ota_busy = true;

            ESP_LOGI(TAG, "Downloading new firmware");
        }

        ESP_ERROR_CHECK_JUMP(esp_ota_write(update_handle, (const void *)buffer, read), end);

        firmware_size += read;

        ESP_LOGD(TAG, "Written %d bytes, total %d", read, firmware_size);
    }

    if (!esp_http_client_is_complete_data_received(client)) {
        ESP_LOGE(TAG, "Stream not fully read");
        goto end;
    }

    ESP_ERROR_CHECK_JUMP(esp_ota_end(update_handle), end);

    ota_busy = false;
    firmware_installed = true;

end:
    if (ota_busy) {
        esp_ota_abort(update_handle);
    }

    delete[] buffer;

    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    return firmware_installed;
}

bool OTAManager::parse_hash(char *buffer, uint8_t *hash) {
    for (auto i = 0; i < HASH_LENGTH; i++) {
        auto h = hextoi(buffer[i * 2]);
        auto l = hextoi(buffer[i * 2 + 1]);

        if (h == -1 || l == -1) {
            return false;
        }

        hash[i] = h << 4 | l;
    }

    return true;
}

std::optional<std::string> OTAManager::get_ota_endpoint() {
    std::string ota_endpoint = CONFIG_OTA_ENDPOINT;

    NetworkConfiguration network_conf;
    if (network_conf.load()) {
        ota_endpoint = network_conf.get_ota_endpoint();
    }

    if (ota_endpoint.empty()) {
        ESP_LOGE(TAG, "Missing OTA endpoint");
        return std::nullopt;
    }

    return ota_endpoint;
}
