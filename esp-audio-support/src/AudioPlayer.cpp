#include "support.h"

#include "AudioPlayer.h"

#include <cinttypes>

#include "esp_heap_caps.h"
#include "esp_wifi.h"

LOG_TAG(AudioPlayer);

// A stream that keeps running dry would log a line each time, a few a second, and esp-mdm ships every
// line over MQTT. So an underrun logs at most once in this time, with the count since the last line.
static constexpr int64_t UNDERRUN_LOG_INTERVAL_US = 10 * 1000 * 1000;

// What a stream counted from its start to its end.
static StreamBufferStats since(const StreamBufferStats& now, const StreamBufferStats& then) {
    return {
        .packets = now.packets - then.packets,
        .gaps_filled = now.gaps_filled - then.gaps_filled,
        .reordered = now.reordered - then.reordered,
        .late_or_duplicate = now.late_or_duplicate - then.late_or_duplicate,
        .overflows = now.overflows - then.overflows,
        .invalid = now.invalid - then.invalid,
        .new_streams = now.new_streams - then.new_streams,
        .underruns = now.underruns - then.underruns,
    };
}

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

        const auto before = _buffer->stats();

        result = _buffer->push(source, packet, length);

        // The first packet the buffer plays starts the stream that fill() logs the end of.
        if (!_streaming && _buffer->playing()) {
            _streaming = true;
            _stream_start_stats = before;
            _stream_start_us = esp_timer_get_time();
        }

        // AutoVolume's envelope is the last stream's, and a new one starts from its own level.
        if (result == PushResult::NewStream && _auto_volume) {
            _auto_volume->reset();
        }
    }

    _output.start();

    return result;
}

esp_err_t AudioPlayer::listen(uint16_t port) {
    ESP_ASSERT_RETURN(_ring, ESP_ERR_INVALID_STATE);
    ESP_ASSERT_RETURN(!_receiver.is_started(), ESP_ERR_INVALID_STATE);

    const auto err = esp_wifi_set_ps(WIFI_PS_NONE);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "Wi-Fi modem power save off, so the stream's packets come as they are sent");
    } else {
        ESP_LOGW(TAG, "Turning Wi-Fi modem power save off failed: %s", esp_err_to_name(err));
    }

    const UDPReceiverConfig receiver_config = {
        .port = port,
        .task_priority = _config.receiver_task_priority,
        .task_core = _config.receiver_task_core,
    };

    return _receiver.begin(receiver_config, [this](const sockaddr_in& sender, const uint8_t* data, size_t length) {
        on_datagram(sender, data, length);
    });
}

SourceId AudioPlayer::source_of(const sockaddr_in& sender) {
    return (SourceId)ntohl(sender.sin_addr.s_addr) << 16 | ntohs(sender.sin_port);
}

void AudioPlayer::on_datagram(const sockaddr_in& sender, const uint8_t* data, size_t length) {
    const auto result = push(source_of(sender), data, length);
    if (result != PushResult::NewStream && result != PushResult::Resumed) {
        return;
    }

    const auto ip = ntohl(sender.sin_addr.s_addr);
    const auto counter = length >= StreamBuffer::HEADER_BYTES
                             ? (uint32_t)data[0] << 24 | (uint32_t)data[1] << 16 | (uint32_t)data[2] << 8 | data[3]
                             : 0;

    if (result == PushResult::NewStream) {
        ESP_LOGI(TAG, "Stream from %" PRIu32 ".%" PRIu32 ".%" PRIu32 ".%" PRIu32 ":%u started, at counter %" PRIu32,
                 ip >> 24, (ip >> 16) & 0xff, (ip >> 8) & 0xff, ip & 0xff, ntohs(sender.sin_port), counter);
        return;
    }

    _underruns_unlogged++;

    const auto now = esp_timer_get_time();
    if (_underrun_logged_us && now - _underrun_logged_us < UNDERRUN_LOG_INTERVAL_US) {
        return;
    }

    ESP_LOGW(TAG,
             "Stream from %" PRIu32 ".%" PRIu32 ".%" PRIu32 ".%" PRIu32 ":%u ran dry and goes on, at counter %" PRIu32
             ": an underrun, %" PRIu32 " since the last line like this",
             ip >> 24, (ip >> 16) & 0xff, (ip >> 8) & 0xff, ip & 0xff, ntohs(sender.sin_port), counter,
             _underruns_unlogged);

    _underruns_unlogged = 0;
    _underrun_logged_us = now;
}

AudioPlayerStats AudioPlayer::get_stats() const {
    auto lock = _lock.take();

    return {
        .stream = _buffer->stats(),
        .buffered_samples = _buffer->buffered_samples(),
        .output = _output.get_stats(),
        .receiver = _receiver.get_stats(),
    };
}

bool AudioPlayer::fill(int16_t* block, size_t count) {
    std::optional<StreamBufferStats> ended;
    int64_t ended_after_us = 0;
    bool filled = false;

    {
        auto lock = _lock.take();

        // The stream buffer has run dry and waited out its grace: the stream has ended.
        if (_streaming && !_buffer->playing()) {
            _streaming = false;
            ended = since(_buffer->stats(), _stream_start_stats);
            ended_after_us = esp_timer_get_time() - _stream_start_us;
        }

        // The stream buffer plays through its grace, so a late packet keeps the output running on
        // silence.
        if (_buffer->playing() || _tone->playing()) {
            _buffer->read(block, count);

            if (_auto_volume) {
                _auto_volume->process(block, count);
            }

            _tone->mix(block, count);
            _gain.process(block, count);

            filled = true;
        }
    }

    if (ended.has_value()) {
        ESP_LOGI(TAG,
                 "Stream ended after %.1f s, its grace included: %" PRIu32 " packets, %" PRIu32 " gaps filled, %" PRIu32
                 " reordered, %" PRIu32 " late or duplicate, %" PRIu32 " overflows, %" PRIu32 " invalid, %" PRIu32
                 " new streams, %" PRIu32 " underruns",
                 (double)ended_after_us / 1e6, ended->packets, ended->gaps_filled, ended->reordered,
                 ended->late_or_duplicate, ended->overflows, ended->invalid, ended->new_streams, ended->underruns);
    }

    return filled;
}
