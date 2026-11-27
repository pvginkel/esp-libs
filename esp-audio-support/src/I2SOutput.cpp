#include "support.h"

#include "I2SOutput.h"

#include <cinttypes>
#include <cstring>

#include "esp_attr.h"
#include "esp_heap_caps.h"

LOG_TAG(I2SOutput);

// The DMA's buffers: ESP-IDF's defaults, 6 of 240 frames, which is 30 ms at 48 kHz.
static constexpr uint32_t DMA_BUFFERS = 6;
static constexpr uint32_t DMA_BUFFER_FRAMES = 240;

// A write that has not gone through in this time means the channel has stopped.
static constexpr uint32_t WRITE_TIMEOUT_MS = 1000;

static constexpr uint32_t TASK_STACK_SIZE = 4096;

esp_err_t I2SOutput::begin(const I2SOutputConfig& config, Source source, I2SOutputTap tap) {
    ESP_ASSERT_RETURN(!_task, ESP_ERR_INVALID_STATE);
    ESP_ASSERT_RETURN(source, ESP_ERR_INVALID_ARG);
    ESP_ASSERT_RETURN(config.sample_rate_hz > 0 && config.block_ms > 0, ESP_ERR_INVALID_ARG);

    _config = config;
    _source = std::move(source);
    _tap = std::move(tap);

    _block_samples = (size_t)((uint64_t)config.sample_rate_hz * config.block_ms / 1000);
    _silence_samples = DMA_BUFFER_FRAMES;
    _dma_buffers = DMA_BUFFERS;

    ESP_ASSERT_RETURN(_block_samples > 0, ESP_ERR_INVALID_ARG);

    // The task copies from these into the DMA's buffers, so they need not be DMA-capable.
    _block = (int16_t*)heap_caps_calloc(_block_samples, sizeof(int16_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    _silence = (int16_t*)heap_caps_calloc(_silence_samples, sizeof(int16_t), MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    ESP_ASSERT_RETURN(_block && _silence, ESP_ERR_NO_MEM);

    i2s_chan_config_t channel_config = I2S_CHANNEL_DEFAULT_CONFIG(config.port, I2S_ROLE_MASTER);
    channel_config.dma_desc_num = DMA_BUFFERS;
    channel_config.dma_frame_num = DMA_BUFFER_FRAMES;
    // A buffer the task did not refill in time plays as silence, not as what it held a round ago.
    channel_config.auto_clear_after_cb = true;
    ESP_ERROR_RETURN(i2s_new_channel(&channel_config, &_channel, nullptr));

    // Mono: the samples go out in the left slot, the one the MAX98357 plays with its SD pin high.
    i2s_std_slot_config_t slot_config =
        I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO);
    if (config.format == I2SFormat::MSB) {
        slot_config = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO);
    }

    const i2s_std_config_t std_config = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(config.sample_rate_hz),
        .slot_cfg = slot_config,
        .gpio_cfg =
            {
                .mclk = I2S_GPIO_UNUSED,
                .bclk = config.bclk_gpio,
                .ws = config.ws_gpio,
                .dout = config.dout_gpio,
                .din = I2S_GPIO_UNUSED,
                .invert_flags =
                    {
                        .mclk_inv = false,
                        .bclk_inv = false,
                        .ws_inv = false,
                    },
            },
    };
    ESP_ERROR_RETURN(i2s_channel_init_std_mode(_channel, &std_config));

    const i2s_event_callbacks_t callbacks = {
        .on_recv = nullptr,
        .on_recv_q_ovf = nullptr,
        .on_sent = nullptr,
        .on_send_q_ovf = on_send_q_ovf,
    };
    ESP_ERROR_RETURN(i2s_channel_register_event_callback(_channel, &callbacks, this));

    const auto created = xTaskCreatePinnedToCore([](void* arg) { ((I2SOutput*)arg)->run(); }, "i2s_output",
                                                 TASK_STACK_SIZE, this, config.task_priority, &_task, config.task_core);
    ESP_ASSERT_RETURN(created == pdPASS, ESP_ERR_NO_MEM);

    ESP_LOGI(TAG,
             "I2S%d: %s, 16-bit mono in the left slot, %" PRIu32 " Hz, on BCLK %d, WS %d, DOUT %d; %" PRIu32
             " ms blocks, %" PRId64 " ms of DMA",
             (int)config.port, config.format == I2SFormat::MSB ? "MSB" : "Philips", config.sample_rate_hz,
             (int)config.bclk_gpio, (int)config.ws_gpio, (int)config.dout_gpio, config.block_ms,
             samples_to_us(_dma_buffers * _silence_samples) / 1000);

    return ESP_OK;
}

void I2SOutput::start() {
    if (_task) {
        xTaskNotifyGive(_task);
    }
}

I2SOutputStats I2SOutput::get_stats() const {
    return {
        .starts = _starts,
        .underruns = _underruns,
        .stack_spare_bytes = _task ? (uint32_t)uxTaskGetStackHighWaterMark(_task) : 0,
    };
}

void I2SOutput::run() {
    while (true) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

        // A start that came while it played, or for a source that has emptied since, finds nothing.
        if (_source(_block, _block_samples)) {
            play();
        }
    }
}

void I2SOutput::play() {
    // Silence fills the DMA first, so the amplifier has its clocks before the first sample.
    size_t preloaded_bytes = 0;
    while (true) {
        size_t loaded = 0;
        ESP_ERROR_CHECK(i2s_channel_preload_data(_channel, _silence, _silence_samples * sizeof(int16_t), &loaded));
        preloaded_bytes += loaded;
        if (loaded < _silence_samples * sizeof(int16_t)) {
            break;
        }
    }

    if (_tap.reset) {
        _tap.reset();
    }

    const uint32_t underruns_before = _underruns;

    _starts++;
    _playing = true;

    ESP_LOGI(TAG, "Started");

    ESP_ERROR_CHECK(i2s_channel_enable(_channel));

    // The first block plays once the preload has.
    auto time_us = esp_timer_get_time() + samples_to_us(preloaded_bytes / sizeof(int16_t));
    uint64_t played = 0;

    do {
        if (_tap.samples) {
            _tap.samples(_block, _block_samples, time_us);
        }
        time_us += samples_to_us(_block_samples);

        if (!write(_block, _block_samples)) {
            break;
        }
        played += _block_samples;
    } while (_source(_block, _block_samples));

    // The drain: the DMA holds the last blocks still, so silence pushes them out before the clocks stop.
    // One buffer more than the DMA has covers the one it is sending.
    for (size_t i = 0; i <= _dma_buffers; i++) {
        if (!write(_silence, _silence_samples)) {
            break;
        }
    }

    ESP_ERROR_CHECK(i2s_channel_disable(_channel));

    _playing = false;

    if (_tap.reset) {
        _tap.reset();
    }

    ESP_LOGI(TAG, "Stopped after %" PRId64 " ms played, %" PRIu32 " DMA underruns; %" PRIu32 " bytes of stack spare",
             samples_to_us(played) / 1000, _underruns - underruns_before,
             (uint32_t)uxTaskGetStackHighWaterMark(nullptr));
}

bool I2SOutput::write(const int16_t* samples, size_t count) {
    size_t written = 0;
    const auto err = i2s_channel_write(_channel, samples, count * sizeof(int16_t), &written, WRITE_TIMEOUT_MS);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Write failed after %zu of %zu bytes: %s", written, count * sizeof(int16_t),
                 esp_err_to_name(err));
        return false;
    }

    return true;
}

int64_t I2SOutput::samples_to_us(uint64_t samples) const {
    return (int64_t)(samples * 1000000 / _config.sample_rate_hz);
}

// The DMA came round to a buffer the task had not refilled. In the ISR.
bool IRAM_ATTR I2SOutput::on_send_q_ovf(i2s_chan_handle_t channel, i2s_event_data_t* event, void* context) {
    ((I2SOutput*)context)->_underruns.fetch_add(1, std::memory_order_relaxed);

    return false;
}
