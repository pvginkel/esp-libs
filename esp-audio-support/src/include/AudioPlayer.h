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
#include "UDPReceiver.h"

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
    // The receiver's task, which listen() starts. Above the output's (Intercom's finding H5).
    UBaseType_t receiver_task_priority = 8;
    BaseType_t receiver_task_core = tskNO_AFFINITY;
};

// What an AudioPlayer has counted since it began.
struct AudioPlayerStats {
    StreamBufferStats stream;
    // How far the stream reaches ahead of what plays.
    size_t buffered_samples;
    I2SOutputStats output;
    // All zero until listen().
    UDPReceiverStats receiver;
};

// The speaker: the stream buffer and a tone, mixed, through the volume, out on I2S.
//
// Each block the output asks for is the stream buffer's next, then AutoVolume when it is on, then the
// tone mixed in, then the volume. The output plays while the stream buffer has a stream, its grace
// included, or the tone plays, and stops when neither has anything.
//
// listen() opens the UDP port the stream comes in on: the first packet starts the output, and the
// output stops once the stream has run dry and its grace has passed.
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
    UDPReceiver _receiver;
    mutable Mutex _lock;
    // Whether the stream buffer has had a stream since fill() last saw it idle, with the counters and the
    // time when it began, for the line that logs its end.
    bool _streaming{};
    StreamBufferStats _stream_start_stats{};
    int64_t _stream_start_us{};
    // The underruns since the last line that logged one, and when that was. On the receiver's task.
    uint32_t _underruns_unlogged{};
    int64_t _underrun_logged_us{};

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

    // Opens UDP `port` on a task of its own, and pushes each datagram as a packet, its source the
    // sender's IPv4 address and port. Call it once, after begin(), once the network is up.
    //
    // It turns Wi-Fi modem power save off first, for good: with it on, the access point holds the
    // packets until the next beacon, and they come in bursts with gaps of 100 to 300 ms (Intercom's
    // finding H1).
    esp_err_t listen(uint16_t port);

    bool is_playing() const { return _output.is_playing(); }
    AudioPlayerStats get_stats() const;
    const AudioPlayerConfig& get_config() const { return _config; }

    // The source a sender's packets push under: its IPv4 address and port.
    static SourceId source_of(const sockaddr_in& sender);

private:
    bool fill(int16_t* block, size_t count);
    void on_datagram(const sockaddr_in& sender, const uint8_t* data, size_t length);
};
