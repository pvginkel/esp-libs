#pragma once

#include <stdint.h>

#include <string>

#include "Callback.h"
#include "MDMConfiguration.h"
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
    bool install_firmware(OTAConfig& ota_config);
    bool parse_hash(char* buffer, uint8_t* hash);
};
