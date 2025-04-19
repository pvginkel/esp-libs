#include "support.h"

#include "NetworkConfiguration.h"

#include <iostream>
#include <sstream>

#include "esp_log.h"
#include "esp_partition.h"

LOG_TAG(NetworkConfiguration);

using namespace std;

bool NetworkConfiguration::load() {
    const auto partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, "netw_conf");
    if (!partition) {
        ESP_LOGW(TAG, "No netw_conf partition found");
        return false;
    }

    const void* data;
    esp_partition_mmap_handle_t map_handle;
    ESP_ERROR_CHECK(esp_partition_mmap(partition, 0, partition->size, ESP_PARTITION_MMAP_DATA, &data, &map_handle));

    string content((const char*)data, partition->size);
    istringstream stream(content);
    string line;

    while (getline(stream, line)) {
        auto pos = line.find('=');
        if (pos == string::npos) {
            continue;
        }

        auto key = line.substr(0, pos);
        auto value = line.substr(pos + 1);

        if (key == "OTA_ENDPOINT") {
            _ota_endpoint = value;
        }
    }

    esp_partition_munmap(map_handle);

    return !_ota_endpoint.empty();
}
