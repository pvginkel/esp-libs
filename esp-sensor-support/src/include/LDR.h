#pragma once

#include <cstdint>

#include "Callback.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_continuous.h"
#include "esp_err.h"
#include "hal/adc_types.h"

class LDR {
    Queue* _queue{};
    float _divider_resistance{};
    float _resistance_at_10_lux{};
    float _gamma{};
    adc_channel_t _channel{};

    adc_continuous_handle_t _adc_handle{};
    adc_cali_handle_t _cali_handle{};

    uint64_t _sum{};
    size_t _sample_count{};
    size_t _samples_needed{};

    Callback<float> _lux_changed;

public:
    // divider_resistance: the fixed resistor in the voltage divider (ohms)
    // resistance_at_10_lux: the LDR resistance at 10 lux (ohms), from the datasheet
    // gamma: LDR gamma coefficient (typically 0.6-0.8)
    LDR(Queue* queue, float divider_resistance, float resistance_at_10_lux, float gamma)
        : _queue(queue),
          _divider_resistance(divider_resistance),
          _resistance_at_10_lux(resistance_at_10_lux),
          _gamma(gamma) {}

    esp_err_t begin(int pin, uint32_t report_interval_ms);

    void on_lux_changed(std::function<void(float)> func) { _lux_changed.add(func); }

private:
    void task_loop();
    void process_samples(const uint8_t* buffer, uint32_t len);
    float mv_to_lux(float mv);
};
