#pragma once

class ZigBeeCommandFilter {
public:
    virtual esp_err_t prefilterCommonCommand(uint8_t param) { return ESP_OK; }
    virtual esp_err_t prefilterSpecificCommand(uint8_t param) { return ESP_OK; }
};

class ZigBeeTransitionTimeCommandFilter : public ZigBeeCommandFilter {
    uint16_t _transition_time{};

public:
    esp_err_t prefilterSpecificCommand(uint8_t param);

    uint16_t getTransitionTime() { return _transition_time; }
};
