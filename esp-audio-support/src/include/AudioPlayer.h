#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

#include "AutoVolume.h"
#include "Gain.h"
#include "I2SOutput.h"
#include "Mutex.h"
#include "StreamBuffer.h"
#include "ToneSource.h"

// The configuration of an AudioPlayer.
struct AudioPlayerConfig {
    // The I2S output: its port, its pins and its format.
    i2s_port_t i2s_port = I2S_NUM_0;
    gpio_num_t bclk_gpio = GPIO_NUM_NC;
    gpio_num_t ws_gpio = GPIO_NUM_NC;
    gpio_num_t dout_gpio = GPIO_NUM_NC;
    I2SFormat format = I2SFormat::PHILIPS;
    // The stream: 16-bit mono at this rate. The depth, the lead and the grace are StreamBufferConfig's.
    uint32_t sample_rate_hz = 48000;
    uint32_t depth_ms = 1000;
    uint32_t lead_ms = 200;
    uint32_t grace_ms = 500;
    // Whether the stream buffer's ring is in PSRAM rather than internal RAM. It takes 96 KB a second of
    // depth at 48 kHz.
    bool buffer_in_psram = false;
    // The UDP port the stream comes in on. Nothing opens it yet.
    uint16_t udp_port = 11106;
    // The volume it starts at, and the range in dB it maps onto (Gain).
    float volume = 1.0f;
    float min_db = Gain::DEFAULT_MIN_DB;
    float max_db = Gain::DEFAULT_MAX_DB;
    // AutoVolume on the stream, before the tone is mixed in and the volume is applied.
    bool auto_volume = false;
    float auto_volume_target_db = -18.0f;
    // The output's task.
    UBaseType_t task_priority = 7;
    BaseType_t task_core = tskNO_AFFINITY;
};

// What an AudioPlayer has counted since it began.
struct AudioPlayerStats {
    StreamBufferStats stream;
    // How far the stream reaches ahead of what plays.
    size_t buffered_samples;
    I2SOutputStats output;
};

// The speaker: the stream buffer and a tone, mixed, through the volume, out on I2S.
//
// Each block the output asks for is the stream buffer's next, then AutoVolume when it is on, then the
// tone mixed in, then the volume. The output plays while the stream buffer has a stream, its grace
// included, or the tone plays, and stops when neither has anything.
//
// Every function may be called from any task: a lock guards the buffer, the tone and the volume
// against the output's task.
class AudioPlayer {
    AudioPlayerConfig _config;
    int16_t* _ring{};
    std::optional<StreamBuffer> _buffer;
    std::optional<ToneSource> _tone;
    std::optional<AutoVolume> _auto_volume;
    Gain _gain;
    I2SOutput _output;
    mutable Mutex _lock;

public:
    AudioPlayer() = default;
    AudioPlayer(const AudioPlayer&) = delete;
    AudioPlayer& operator=(const AudioPlayer&) = delete;

    // Allocates the ring and starts the output, idle. The tap is the output's (I2SOutputTap).
    esp_err_t begin(const AudioPlayerConfig& config, I2SOutputTap tap = {});

    // The volume, 0 to 1, clamped. 0 mutes.
    void set_volume(float volume);
    float get_volume() const;
    // The gain the volume maps to: negative infinity when muted.
    float get_volume_db() const;

    // Plays `count` bursts of a sine, `pause_ms` apart, over whatever else plays, at a peak of `level`
    // (0 to 1 of full scale) before the volume. It replaces a tone that is still playing.
    void play_tone(float frequency_hz, uint32_t duration_ms, float level, uint32_t count = 1, uint32_t pause_ms = 0);

    // A stream's packet: a 4-byte big-endian counter, then 16-bit little-endian PCM (StreamBuffer).
    PushResult push(SourceId source, const uint8_t* packet, size_t length);

    bool is_playing() const { return _output.is_playing(); }
    AudioPlayerStats get_stats() const;
    const AudioPlayerConfig& get_config() const { return _config; }

private:
    bool fill(int16_t* block, size_t count);
};
