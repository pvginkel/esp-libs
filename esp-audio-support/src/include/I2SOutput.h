#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>

#include "driver/i2s_std.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// The I2S data format. The MAX98357 takes either: ToiletAutomation's board was brought up on Philips,
// and Intercom's boards play MSB.
enum class I2SFormat {
    PHILIPS,
    MSB,
};

// The configuration of an I2SOutput: 16-bit mono, sent in the left slot.
struct I2SOutputConfig {
    i2s_port_t port = I2S_NUM_0;
    gpio_num_t bclk_gpio = GPIO_NUM_NC;
    gpio_num_t ws_gpio = GPIO_NUM_NC;
    gpio_num_t dout_gpio = GPIO_NUM_NC;
    I2SFormat format = I2SFormat::PHILIPS;
    uint32_t sample_rate_hz = 48000;
    // How much the task takes from its source and writes at a time.
    uint32_t block_ms = 20;
    // The output's task. It does little work a block, but a late block is heard.
    UBaseType_t task_priority = 7;
    BaseType_t task_core = tskNO_AFFINITY;
};

// What an I2SOutput has counted since it began.
struct I2SOutputStats {
    // Times it started playing.
    uint32_t starts{};
    // DMA buffers that came round before the task had refilled them. Each played as silence.
    uint32_t underruns{};
    // The least stack its task has had spare, in bytes.
    uint32_t stack_spare_bytes{};
};

// The played-samples tap: what the output plays, for a consumer such as an echo canceller's
// reference. Both run on the output's task.
struct I2SOutputTap {
    // The samples just before they are written, and the esp_timer time when the first of them plays.
    std::function<void(const int16_t* samples, size_t count, int64_t time_us)> samples;
    // The output starts or has stopped: the samples after this do not follow on from the ones before.
    std::function<void()> reset;
};

// The speaker's I2S channel, TX only, on its own task.
//
// The task sleeps until start(). It then takes a block from its source, preloads the DMA with silence,
// enables the channel and writes the block. The blocking write sets the pace from there. When the
// source has nothing more, the task writes the DMA's depth of silence, so the last block plays out
// before the clocks stop, and disables the channel, which mutes the amplifier.
//
// It has no grace of its own: a source that waits for a late packet, as AudioPlayer's StreamBuffer
// does, returns silence meanwhile and keeps it playing.
class I2SOutput {
public:
    // Fills `block` with the next `count` samples, or is false when there is nothing to play, and leaves
    // it as it is. It runs on the output's task.
    using Source = std::function<bool(int16_t* block, size_t count)>;

private:
    I2SOutputConfig _config;
    Source _source;
    I2SOutputTap _tap;
    i2s_chan_handle_t _channel{};
    TaskHandle_t _task{};
    size_t _block_samples{};
    int16_t* _block{};
    // One DMA buffer of silence, for the preload and the drain.
    size_t _silence_samples{};
    int16_t* _silence{};
    size_t _dma_buffers{};
    std::atomic<bool> _playing{};
    std::atomic<uint32_t> _starts{};
    std::atomic<uint32_t> _underruns{};

public:
    I2SOutput() = default;
    I2SOutput(const I2SOutput&) = delete;
    I2SOutput& operator=(const I2SOutput&) = delete;

    // Sets the channel up and starts the task, idle. The tap's functions are optional.
    esp_err_t begin(const I2SOutputConfig& config, Source source, I2SOutputTap tap = {});

    // Wakes the task, which plays until its source has nothing more. Any task may call it, at any time:
    // a start while it plays, or with nothing to play, does nothing.
    void start();

    bool is_playing() const { return _playing; }
    I2SOutputStats get_stats() const;

    const I2SOutputConfig& get_config() const { return _config; }
    size_t get_block_samples() const { return _block_samples; }

private:
    void run();
    void play();
    bool write(const int16_t* samples, size_t count);
    int64_t samples_to_us(uint64_t samples) const;
    static bool on_send_q_ovf(i2s_chan_handle_t channel, i2s_event_data_t* event, void* context);
};
