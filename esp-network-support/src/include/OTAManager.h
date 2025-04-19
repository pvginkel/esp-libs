#pragma once

#include <stdint.h>

#include <optional>
#include <string>

#include "Callback.h"
#include "esp_ota_ops.h"
#include "esp_timer.h"

class OTAManager {
    struct OTAConfig {
        const char* endpoint;
        const esp_partition_t* update_partition;
        const esp_partition_t* running_partition;
        bool check_only;
        bool force;
    };

    esp_timer_handle_t _update_timer;
    Callback<void> _ota_start;
    const char* _boot_endpoint{};
    const char* _app_endpoint{};

public:
    OTAManager();

    void begin();
    void bootstrap();
    void on_ota_start(std::function<void()> func) { _ota_start.add(func); }

private:
    void update_check();
    bool install_update();
    bool install_firmware(OTAConfig& ota_config);
    bool parse_hash(char* buffer, uint8_t* hash);
    std::optional<std::string> get_ota_endpoint();
};
