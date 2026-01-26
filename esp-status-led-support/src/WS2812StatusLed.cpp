#include "support.h"

#include "WS2812StatusLed.h"

#include <cmath>

#include "led_strip.h"

LOG_TAG(WS2812StatusLed);

void WS2812StatusLed::begin() {
    led_strip_config_t strip_config = {
        .strip_gpio_num = CONFIG_STATUS_LED_WS2812_PIN,
        .max_leds = 1,
        .led_model = LED_MODEL_WS2812,
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_RGB,
        .flags =
            {
                .invert_out = false,
            },
    };

    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,
        .resolution_hz = 10 * 1000 * 1000,  // 10MHz
        .mem_block_symbols = 0,             // Let driver choose automatically
        .flags =
            {
                .with_dma = false,
            },
    };

    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &_led));

    // Start with LED off
    led_strip_clear(_led);

    ESP_LOGI(TAG, "Initialized on GPIO %d", CONFIG_STATUS_LED_WS2812_PIN);
}

void WS2812StatusLed::process() {
    const auto mode = get_mode();

    if (mode == StatusLedMode::Off) {
        return;
    }

    const auto now = esp_get_millis();

    // Not yet time for update
    if (_next_update_ms != 0 && now < _next_update_ms) {
        return;
    }

    const auto period = get_period();

    switch (mode) {
        case StatusLedMode::Continuous:
            // Timed duration elapsed, turn off and reset mode
            set_mode(StatusLedMode::Off);
            break;

        case StatusLedMode::Blinking:
            _blink_state = !_blink_state;
            update_led(_blink_state ? 1.0f : 0.0f);
            _next_update_ms = now + period / 2;
            break;

        case StatusLedMode::Pulsating: {
            const uint32_t elapsed = now - _phase_start_ms;
            const float phase = (float)(elapsed % period) / (float)period;
            const float intensity = (std::sin(phase * 2.0f * M_PI - M_PI_2) + 1.0f) / 2.0f;

            update_led(intensity);
            _next_update_ms = now + 10;  // Update every 10ms for smooth animation
            break;
        }
    }
}

void WS2812StatusLed::config_changed() {
    const auto period = get_period();

    _phase_start_ms = esp_get_millis();
    _blink_state = false;
    _next_update_ms = 0;

    switch (get_mode()) {
        case StatusLedMode::Off:
            update_led(0);
            break;

        case StatusLedMode::Continuous:
            update_led(1.0f);
            // If period > 0, schedule turn-off after duration
            if (period > 0) {
                _next_update_ms = _phase_start_ms + period;
            }
            break;

        case StatusLedMode::Blinking:
        case StatusLedMode::Pulsating:
            ESP_ASSERT_CHECK(period > 0);

            // Force immediate update on next cycle
            _next_update_ms = 1;
            break;
    }
}

void WS2812StatusLed::update_led(float intensity) {
    const auto color = get_color();
    const auto brightness = get_brightness();

    const float scale = intensity * brightness;

    const uint8_t r = (uint8_t)(color.r * scale * 255.0f);
    const uint8_t g = (uint8_t)(color.g * scale * 255.0f);
    const uint8_t b = (uint8_t)(color.b * scale * 255.0f);

    led_strip_set_pixel(_led, 0, r, g, b);
    led_strip_refresh(_led);
}
