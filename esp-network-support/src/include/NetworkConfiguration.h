#pragma once

#include <string>

class NetworkConfiguration {
    std::string _ota_endpoint;

public:
    bool load();

    const std::string& get_ota_endpoint() const { return _ota_endpoint; }
};
