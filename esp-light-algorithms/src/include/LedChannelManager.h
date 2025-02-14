#pragma once

#include "driver/ledc.h"

class LedChannelManager {
    ledc_timer_bit_t _timer_bit;

public:
    LedChannelManager(ledc_timer_bit_t timer_bit) : _timer_bit(timer_bit) {}

    esp_err_t begin();
    esp_err_t configureChannel(uint8_t pin, ledc_channel_t channel);
    esp_err_t setLevel(ledc_channel_t channel, float level);
};
