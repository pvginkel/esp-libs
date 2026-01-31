#pragma once

#include <stdint.h>

#include <string>

#include "Callback.h"
#include "MDMConfiguration.h"
#include "esp_http_client.h"
#include "esp_ota_ops.h"
#include "esp_timer.h"

class OTAManager {
    struct OTAConfig {
        const char* endpoint;
        const char* authorization;
        const esp_partition_t* update_partition;
        const esp_partition_t* running_partition;
        bool check_only;
        bool force;
    };

public:
    bool install_update(const std::string& firmware_url, const std::string& authorization);

private:
    esp_err_t install_firmware(OTAConfig& ota_config, bool& firmware_installed);
    bool parse_hash(char* buffer, uint8_t* hash);
};
