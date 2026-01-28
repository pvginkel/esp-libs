#pragma once

#include "RGBStatusLed.h"
#include "led_strip.h"

class WS2812StatusLed : public RGBStatusLed, public BrightnessStatusLed {
    led_strip_handle_t _led{};
    int64_t _next_update_ms{};  // 0 = disabled, 1 = immediate, else = scheduled time
    int64_t _phase_start_ms{};
    bool _blink_state{};

public:
    void begin() override;
    void process() override;

protected:
    void config_changed() override;

private:
    void update_led(float intensity);
};
