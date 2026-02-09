#include "support.h"

#include "ACS725.h"

#include <cmath>
#include <cstdlib>

#include "NVSProperty.h"
#include "freertos/portmacro.h"
#include "freertos/task.h"
#include "soc/soc_caps.h"

LOG_TAG(ACS725);

static NVSPropertyF32 nvs_zero_mv("zero_mv");
static NVSPropertyF32 nvs_noise_floor_a("noise_a");

// -------------------- USER-TUNABLE COMPILE-TIME CONFIG --------------------

// We sample off of a base frequency of 1 kHz. The multiplier increases this.
#define ACS725_SAMPLE_FREQ_MULTIPLIER 4

// ADC attenuation / bitwidth
#define ACS725_ADC_ATTEN ADC_ATTEN_DB_12
#define ACS725_ADC_BITWIDTH SOC_ADC_DIGI_MAX_BITWIDTH

// Sensitivity (mV/A): depends on the exact ACS725 variant.
// The one in use is ACS725LLCTR-10AB-T which has 132 mV/A.
#define ACS725_SENSITIVITY_MV_PER_A 132.0f

// Continuous mode configuration
#define ACS725_SAMPLE_FREQ_HZ (1000 * ACS725_SAMPLE_FREQ_MULTIPLIER)
#define ACS725_DMA_BUFFER_SIZE 256
#define ACS725_DMA_BUFFER_COUNT 4

// Constants used for calculations.
#define CLIP_LOW_MV 50
#define CLIP_HIGH_MV 3250

// Used in logging only; not reported back over MQTT.
#define ASSUMED_VOLTAGE 230

// Overrides the calibrated noise floor. Calibration values are not
// used at all in calculating the values reported over MQTT.
// They're just informative. The value below however is based
// off of empirical measurements.
#define NOISE_FLOOR 0.013f

// -------------------------------------------------------------------------

esp_err_t ACS725::begin(int pin, uint32_t report_interval_ms) {
    if (report_interval_ms == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    adc_unit_t unit;
    ESP_RETURN_ON_ERROR(adc_continuous_io_to_channel(pin, &unit, &_channel), TAG,
                        "Failed to get ADC channel for GPIO %d", pin);

    _ring_size = (report_interval_ms * ACS725_SAMPLE_FREQ_HZ) / 1000;

    ESP_LOGI(TAG, "Configuring ACS725 frequency %" PRIu32 "Hz report interval %" PRIu32 "ms ring size %" PRIu32,
             uint32_t(ACS725_SAMPLE_FREQ_HZ), report_interval_ms, _ring_size);

    _ring = (float*)calloc(_ring_size, sizeof(float));
    if (!_ring) {
        return ESP_ERR_NO_MEM;
    }

    // ADC continuous mode init
    adc_continuous_handle_cfg_t adc_config = {
        .max_store_buf_size = ACS725_DMA_BUFFER_SIZE * ACS725_DMA_BUFFER_COUNT,
        .conv_frame_size = ACS725_DMA_BUFFER_SIZE,
    };
    ESP_RETURN_ON_ERROR(adc_continuous_new_handle(&adc_config, &_adc_handle), TAG, "adc_continuous_new_handle failed");

    // Configure the ADC pattern (single channel)
    adc_digi_pattern_config_t pattern = {
        .atten = ACS725_ADC_ATTEN,
        .channel = _channel,
        .unit = unit,
        .bit_width = ACS725_ADC_BITWIDTH,
    };

    adc_continuous_config_t dig_cfg = {
        .pattern_num = 1,
        .adc_pattern = &pattern,
        .sample_freq_hz = ACS725_SAMPLE_FREQ_HZ,
        .conv_mode = ADC_CONV_SINGLE_UNIT_1,
        .format = ADC_DIGI_OUTPUT_FORMAT_TYPE2,
    };
    ESP_RETURN_ON_ERROR(adc_continuous_config(_adc_handle, &dig_cfg), TAG, "adc_continuous_config failed");

    // ADC calibration init (required for mV conversion)
    if (!adc_calibration_init(unit, ACS725_ADC_ATTEN, &_cali_handle)) {
        ESP_LOGE(TAG, "ADC calibration not available; refusing to run (enable a supported cali scheme).");
        return ESP_ERR_NOT_SUPPORTED;
    }

    // Start continuous conversion
    ESP_RETURN_ON_ERROR(adc_continuous_start(_adc_handle), TAG, "adc_continuous_start failed");

    xTaskCreate([](void* self) { ((ACS725*)self)->task_loop(); }, "acs725", CONFIG_MAIN_TASK_STACK_SIZE, this, 2,
                nullptr);

    ESP_LOGI(TAG, "Started: unit=%d channel=%d window=%u samples @ %dHz, SENS=%.1fmV/A", (int)unit, (int)_channel,
             (unsigned)_ring_size, ACS725_SAMPLE_FREQ_HZ, (double)ACS725_SENSITIVITY_MV_PER_A);

    return ESP_OK;
}

void ACS725::task_loop() {
    load_state();

    if (_zero_mv == 0) {
        ESP_LOGI(TAG, "Starting calibration");
        _calibration = new ACS725Calibration({});
        _calibration->reset();
    } else {
        ESP_LOGI(TAG, "Loaded calibration: zero mv %.1fmv noise_floor %.3fA", _zero_mv, _noise_floor_a);
    }

    uint8_t buffer[ACS725_DMA_BUFFER_SIZE];
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

void ACS725::load_state() {
    _zero_mv = 0;
    _noise_floor_a = 0;

    nvs_handle_t handle;
    auto err = nvs_open("acs725", NVS_READONLY, &handle);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        return;
    }
    ESP_ERROR_CHECK(err);

    _zero_mv = nvs_zero_mv.get(handle, 0);
    _noise_floor_a = nvs_noise_floor_a.get(handle, 0);

    nvs_close(handle);
}

void ACS725::save_state() {
    nvs_handle_t handle;
    ESP_ERROR_CHECK(nvs_open("acs725", NVS_READWRITE, &handle));

    nvs_zero_mv.set(handle, _zero_mv);
    nvs_noise_floor_a.set(handle, _noise_floor_a);

    nvs_close(handle);
}

void ACS725::process_samples(const uint8_t* buffer, uint32_t len) {
    auto* data = (adc_digi_output_data_t*)buffer;
    size_t count = len / sizeof(adc_digi_output_data_t);

    for (size_t i = 0; i < count; i++) {
        int raw = data[i].type2.data;
        int mv = 0;

        if (adc_cali_raw_to_voltage(_cali_handle, raw, &mv) != ESP_OK) {
            // A 0 sample is a marker meaning an invalid value. This ensures we
            // do track all samples.
            mv = 0;
        }

        if (_calibration) {
            // Calibration is still in progress.
            const auto done = _calibration->add_sample(mv);
            if (done) {
                const auto result = _calibration->result();

                _zero_mv = result.zero_mv;
                _noise_floor_a = result.final_std_dev / ACS725_SENSITIVITY_MV_PER_A;

                ESP_LOGI(TAG,
                         "Calibration result: success=%s zero_mv=%.1fmv noise floor=%.3fA (defined noise floor=%.3fA) "
                         "standard deviation %.3fmv",
                         result.success ? "YES" : "NO", _zero_mv, _noise_floor_a, NOISE_FLOOR, result.final_std_dev);

                ESP_ASSERT_CHECK(result.success);

                save_state();

                delete _calibration;
                _calibration = nullptr;
            }
            continue;
        }

        // Convert to current (A), then square for RMS calculation
        _ring[_ring_idx++] = mv;

        // Report when the ring buffer is full.
        if (_ring_idx >= _ring_size) {
            _ring_idx = 0;

            // Calculate the mean.
            int clipped = 0;

            uint64_t sum = 0;
            for (size_t i = 0; i < _ring_size; i++) {
                const auto value = _ring[i];
                if (value >= CLIP_LOW_MV && value <= CLIP_HIGH_MV) {
                    sum += value;
                } else {
                    clipped++;
                }
            }

            if (clipped) {
                ESP_LOGW(TAG, "%d samples clipped", clipped);
            }

            const auto mean = (float)sum / (_ring_size - clipped);

            // Calculate the RMS current.
            float current_a_sum = 0;

            for (size_t i = 0; i < _ring_size; i++) {
                const auto value = _ring[i];
                if (value >= CLIP_LOW_MV && value <= CLIP_HIGH_MV) {
                    const float current_a = (value - mean) / ACS725_SENSITIVITY_MV_PER_A;
                    current_a_sum += current_a * current_a;
                }
            }

            const float rms_raw_squared = current_a_sum / (_ring_size - clipped);
            const float rms_raw = std::sqrt(std::max(0.0f, rms_raw_squared));
            const bool report = rms_raw >= 2 * NOISE_FLOOR;
            const float rms = std::sqrt(std::max(0.0f, rms_raw_squared - NOISE_FLOOR * NOISE_FLOOR));

            ESP_LOGI(TAG,
                     "Report %.4fA (raw %.4f, noise floor %.4f, Watt %.1f) mean %.1f mean offset %.1f reporting %s",
                     rms, rms_raw, NOISE_FLOOR, rms * ASSUMED_VOLTAGE, mean, _zero_mv - mean,
                     report ? "YES" : "NO - below threshold of 2 x noise floor");

            _current_changed.queue(_queue, rms);
        }
    }
}
