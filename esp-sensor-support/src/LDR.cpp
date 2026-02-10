#include "support.h"

#include "LDR.h"

#include <cmath>

#include "freertos/portmacro.h"
#include "freertos/task.h"
#include "soc/soc_caps.h"

LOG_TAG(LDR);

// -------------------- USER-TUNABLE COMPILE-TIME CONFIG --------------------

// ADC attenuation / bitwidth
#define LDR_ADC_ATTEN ADC_ATTEN_DB_12
#define LDR_ADC_BITWIDTH SOC_ADC_DIGI_MAX_BITWIDTH

// Continuous mode configuration
#define LDR_SAMPLE_FREQ_HZ SOC_ADC_SAMPLE_FREQ_THRES_LOW
#define LDR_DMA_BUFFER_SIZE 256
#define LDR_DMA_BUFFER_COUNT 4

// Supply voltage in mV
#define LDR_VCC_MV 3300.0f

// -------------------------------------------------------------------------

esp_err_t LDR::begin(int pin, uint32_t report_interval_ms) {
    if (report_interval_ms == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    adc_unit_t unit;
    ESP_RETURN_ON_ERROR(adc_continuous_io_to_channel(pin, &unit, &_channel), TAG,
                        "Failed to get ADC channel for GPIO %d", pin);

    _samples_needed = (report_interval_ms * LDR_SAMPLE_FREQ_HZ) / 1000;
    if (_samples_needed == 0) {
        _samples_needed = 1;
    }

    ESP_LOGI(TAG, "Configuring LDR frequency %dHz report interval %" PRIu32 "ms samples per report %zu",
             LDR_SAMPLE_FREQ_HZ, report_interval_ms, _samples_needed);

    // ADC continuous mode init
    adc_continuous_handle_cfg_t adc_config = {
        .max_store_buf_size = LDR_DMA_BUFFER_SIZE * LDR_DMA_BUFFER_COUNT,
        .conv_frame_size = LDR_DMA_BUFFER_SIZE,
    };
    ESP_RETURN_ON_ERROR(adc_continuous_new_handle(&adc_config, &_adc_handle), TAG, "adc_continuous_new_handle failed");

    // Configure the ADC pattern (single channel)
    adc_digi_pattern_config_t pattern = {
        .atten = LDR_ADC_ATTEN,
        .channel = _channel,
        .unit = unit,
        .bit_width = LDR_ADC_BITWIDTH,
    };

    adc_continuous_config_t dig_cfg = {
        .pattern_num = 1,
        .adc_pattern = &pattern,
        .sample_freq_hz = LDR_SAMPLE_FREQ_HZ,
        .conv_mode = ADC_CONV_SINGLE_UNIT_1,
        .format = ADC_DIGI_OUTPUT_FORMAT_TYPE2,
    };
    ESP_RETURN_ON_ERROR(adc_continuous_config(_adc_handle, &dig_cfg), TAG, "adc_continuous_config failed");

    // ADC calibration init (required for mV conversion)
    if (!adc_calibration_init(unit, LDR_ADC_ATTEN, &_cali_handle)) {
        ESP_LOGE(TAG, "ADC calibration not available; refusing to run (enable a supported cali scheme).");
        return ESP_ERR_NOT_SUPPORTED;
    }

    // Start continuous conversion
    ESP_RETURN_ON_ERROR(adc_continuous_start(_adc_handle), TAG, "adc_continuous_start failed");

    xTaskCreate([](void* self) { ((LDR*)self)->task_loop(); }, "ldr", CONFIG_MAIN_TASK_STACK_SIZE, this, 2, nullptr);

    ESP_LOGI(TAG, "Started: unit=%d channel=%d report every %zu samples @ %dHz", (int)unit, (int)_channel,
             _samples_needed, LDR_SAMPLE_FREQ_HZ);

    return ESP_OK;
}

void LDR::task_loop() {
    uint8_t buffer[LDR_DMA_BUFFER_SIZE];
    uint32_t bytes_read = 0;

    while (true) {
        esp_err_t err = adc_continuous_read(_adc_handle, buffer, sizeof(buffer), &bytes_read, portMAX_DELAY);

        if (err == ESP_OK && bytes_read > 0) {
            process_samples(buffer, bytes_read);
        } else if (err != ESP_ERR_TIMEOUT) {
            ESP_LOGW(TAG, "adc_continuous_read failed: %s", esp_err_to_name(err));
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
}

void LDR::process_samples(const uint8_t* buffer, uint32_t len) {
    auto* data = (adc_digi_output_data_t*)buffer;
    size_t count = len / sizeof(adc_digi_output_data_t);

    for (size_t i = 0; i < count; i++) {
        int raw = data[i].type2.data;
        int mv = 0;

        if (adc_cali_raw_to_voltage(_cali_handle, raw, &mv) != ESP_OK) {
            continue;
        }

        _sum += mv;
        _sample_count++;

        if (_sample_count >= _samples_needed) {
            const float mean_mv = (float)_sum / _sample_count;
            _sum = 0;
            _sample_count = 0;

            const float lux = mv_to_lux(mean_mv);

            ESP_LOGI(TAG, "Report %.1f lux (mean %.1f mV)", lux, mean_mv);

            _lux_changed.queue(_queue, lux);
        }
    }
}

// Assumes voltage divider: VCC --- [LDR] --- [ADC] --- [R_fixed] --- GND
// R_ldr = R_fixed * (VCC - V) / V
// lux = 10 * (R_ldr / R_10)^(-1/gamma)
float LDR::mv_to_lux(float mv) {
    if (mv <= 0 || mv >= LDR_VCC_MV) {
        return 0;
    }

    const float r_ldr = _divider_resistance * (LDR_VCC_MV - mv) / mv;
    if (r_ldr <= 0) {
        return 0;
    }

    return 10.0f * std::pow(r_ldr / _resistance_at_10_lux, -1.0f / _gamma);
}
