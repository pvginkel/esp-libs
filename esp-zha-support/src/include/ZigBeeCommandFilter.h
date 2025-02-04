#pragma once

class ZigBeeCommandFilter {
public:
    virtual esp_err_t prefilterCommand(uint8_t param) = 0;
};

class ZigBeeTransitionTimeCommandFilter : public ZigBeeCommandFilter {
    uint16_t _transition_time{};

public:
    esp_err_t prefilterCommand(uint8_t param);

    uint16_t getTransitionTime() { return _transition_time; }
};
