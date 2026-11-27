#include "support.h"

#include "AudioPlayer.h"

#include <cinttypes>

#include "esp_heap_caps.h"

LOG_TAG(AudioPlayer);

esp_err_t AudioPlayer::begin(const AudioPlayerConfig& config, I2SOutputTap tap) {
    ESP_ASSERT_RETURN(!_ring, ESP_ERR_INVALID_STATE);

    _config = config;

    const StreamBufferConfig buffer_config = {
        .sample_rate_hz = config.sample_rate_hz,
        .depth_ms = config.depth_ms,
        .lead_ms = config.lead_ms,
        .grace_ms = config.grace_ms,
    };

    const auto ring_samples = StreamBuffer::ring_samples(buffer_config);
    const auto ring_bytes = ring_samples * sizeof(int16_t);
    const uint32_t caps = (config.buffer_in_psram ? MALLOC_CAP_SPIRAM : MALLOC_CAP_INTERNAL) | MALLOC_CAP_8BIT;

    _ring = (int16_t*)heap_caps_malloc(ring_bytes, caps);
    if (!_ring) {
        ESP_LOGE(TAG, "No %zu bytes in %s for the ring", ring_bytes, config.buffer_in_psram ? "PSRAM" : "internal RAM");
        return ESP_ERR_NO_MEM;
    }

    _buffer.emplace(buffer_config, _ring);
    _tone.emplace(config.sample_rate_hz);
    _gain = Gain(config.volume, config.min_db, config.max_db);
    if (config.auto_volume) {
        _auto_volume.emplace(config.sample_rate_hz, config.auto_volume_target_db);
    }

    ESP_LOGI(TAG,
             "Ring of %" PRIu32 " ms, %zu bytes in %s; lead %" PRIu32 " ms, grace %" PRIu32
             " ms; volume on %.0f to %.0f dB; AutoVolume %s",
             config.depth_ms, ring_bytes, config.buffer_in_psram ? "PSRAM" : "internal RAM", config.lead_ms,
             config.grace_ms, config.min_db, config.max_db, config.auto_volume ? "on" : "off");

    const I2SOutputConfig output_config = {
        .port = config.i2s_port,
        .bclk_gpio = config.bclk_gpio,
        .ws_gpio = config.ws_gpio,
        .dout_gpio = config.dout_gpio,
        .format = config.format,
        .sample_rate_hz = config.sample_rate_hz,
        .task_priority = config.task_priority,
        .task_core = config.task_core,
    };

    return _output.begin(
        output_config, [this](int16_t* block, size_t count) { return fill(block, count); }, std::move(tap));
}

void AudioPlayer::set_volume(float volume) {
    auto lock = _lock.take();

    _gain.set_volume(volume);
}

float AudioPlayer::get_volume() const {
    auto lock = _lock.take();

    return _gain.get_volume();
}

float AudioPlayer::get_volume_db() const {
    auto lock = _lock.take();

    return _gain.get_db();
}

void AudioPlayer::play_tone(float frequency_hz, uint32_t duration_ms, float level, uint32_t count, uint32_t pause_ms) {
    {
        auto lock = _lock.take();

        _tone->start(frequency_hz, duration_ms, level, count, pause_ms);
    }

    _output.start();
}

PushResult AudioPlayer::push(SourceId source, const uint8_t* packet, size_t length) {
    PushResult result;

    {
        auto lock = _lock.take();

        result = _buffer->push(source, packet, length);

        // AutoVolume's envelope is the last stream's, and a new one starts from its own level.
        if (result == PushResult::NewStream && _auto_volume) {
            _auto_volume->reset();
        }
    }

    _output.start();

    return result;
}

AudioPlayerStats AudioPlayer::get_stats() const {
    auto lock = _lock.take();

    return {
        .stream = _buffer->stats(),
        .buffered_samples = _buffer->buffered_samples(),
        .output = _output.get_stats(),
    };
}

bool AudioPlayer::fill(int16_t* block, size_t count) {
    auto lock = _lock.take();

    // The stream buffer plays through its grace, so a late packet keeps the output running on silence.
    if (!_buffer->playing() && !_tone->playing()) {
        return false;
    }

    _buffer->read(block, count);

    if (_auto_volume) {
        _auto_volume->process(block, count);
    }

    _tone->mix(block, count);
    _gain.process(block, count);

    return true;
}
