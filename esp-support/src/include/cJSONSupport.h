#pragma once

#include "cJSON.h"

class cJSON_Data {
    cJSON* _data;

public:
    cJSON_Data(cJSON* data) : _data(data) {}
    cJSON_Data(const cJSON_Data& other) = delete;
    cJSON_Data(cJSON_Data&& other) noexcept = delete;
    cJSON_Data& operator=(const cJSON_Data& other) = delete;
    cJSON_Data& operator=(cJSON_Data&& other) noexcept = delete;

    ~cJSON_Data() {
        if (_data) {
            cJSON_Delete(_data);
        }
    }

    cJSON* operator*() const { return _data; }
};
