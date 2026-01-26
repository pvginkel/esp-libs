#pragma once

enum class StatusLedMode {
    Off,
    Continuous,
    Blinking,
    Pulsating,
};

class StatusLed {
    StatusLedMode _mode{};
    uint32_t _period{};

public:
    virtual ~StatusLed() {}

    virtual void begin() = 0;
    virtual void process() = 0;

    void set_mode(StatusLedMode mode, uint32_t period = 0) {
        _mode = mode;
        _period = period;

        config_changed();
    }

    StatusLedMode get_mode() const { return _mode; }
    uint32_t get_period() const { return _period; }
    bool is_active() const { return _mode != StatusLedMode::Off; }

protected:
    virtual void config_changed() {}
};

class BrightnessStatusLed : public virtual StatusLed {
    float _brightness{1.0f};

public:
    void set_brightness(float brightness) {
        ESP_ASSERT_CHECK(brightness >= 0.0f && brightness <= 1.0f);

        _brightness = brightness;

        config_changed();
    }

    float get_brightness() const { return _brightness; }
};
