#include "support.h"

#include "LedChannelManager.h"

esp_err_t LedChannelManager::begin() {
    // Prepare and then apply the LEDC PWM timer configuration
    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = _timer_bit,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = 4000,  // Set output frequency at 4 kHz
        .clk_cfg = LEDC_AUTO_CLK,
    };
    return ledc_timer_config(&ledc_timer);
}

esp_err_t LedChannelManager::configureChannel(uint8_t pin, ledc_channel_t channel) {
    // Prepare and then apply the LEDC PWM channel configuration
    ledc_channel_config_t ledc_channel = {
        .gpio_num = pin,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = channel,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,  // Set duty to 0%
        .hpoint = 0,
    };
    return ledc_channel_config(&ledc_channel);
}

esp_err_t LedChannelManager::setLevel(ledc_channel_t channel, float level) {
    auto max_duty_cycle = uint32_t(1) << (int)_timer_bit;
    auto duty_cucle = uint32_t(max_duty_cycle * level);

    auto result = ledc_set_duty(LEDC_LOW_SPEED_MODE, channel, duty_cucle);
    if (result != ESP_OK) {
        return result;
    }

    // Update duty to apply the new value
    return ledc_update_duty(LEDC_LOW_SPEED_MODE, channel);
}
