#pragma once

#include <cstdint>

#include "ACS725Calibration.h"
#include "Callback.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_continuous.h"
#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "hal/adc_types.h"

class ACS725 {
    Queue* _queue{};
    adc_channel_t _channel{};

    adc_continuous_handle_t _adc_handle{};
    adc_cali_handle_t _cali_handle{};

    ACS725Calibration* _calibration{};
    float _zero_mv{};
    float _noise_floor_a{};

    float* _ring{};
    size_t _ring_size{};
    size_t _ring_idx{};

    Callback<float> _current_changed;

public:
    ACS725(Queue* queue) : _queue(queue) {}

    // report_interval_ms: interval at which the current is reported
    esp_err_t begin(int pin, uint32_t report_interval_ms);

    // Register a callback to get notified of RMS current (A) measurement samples.
    void on_current_changed(std::function<void(float)> func) { _current_changed.add(func); }

private:
    void task_loop();
    void load_state();
    void save_state();
    void process_samples(const uint8_t* buffer, uint32_t len);
};
