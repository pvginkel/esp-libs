#pragma once

#include <cstddef>
#include <cstdint>

// A local source: bursts of a sine, for a test tone or a beep. The player mixes it into what it
// reads from the StreamBuffer.
//
// Each burst fades in and out over a few milliseconds, inside its length, so it does not click.
class ToneSource {
public:
    // The longest fade at each end of a burst. A short burst fades over a quarter of its length.
    static constexpr uint32_t FADE_MS = 5;

private:
    uint32_t _sample_rate_hz;
    float _phase_step{};
    float _amplitude{};
    uint64_t _burst_samples{};
    uint64_t _pause_samples{};
    uint64_t _fade_samples{};
    uint32_t _bursts_left{};
    // The position in the current burst and its pause.
    uint64_t _position{};
    float _phase{};

public:
    explicit ToneSource(uint32_t sample_rate_hz);

    // Plays `count` bursts of `duration_ms`, `pause_ms` apart, of a sine at `frequency_hz` with a
    // peak of `level` (0 to 1 of full scale). It replaces a tone that is still playing.
    void start(float frequency_hz, uint32_t duration_ms, float level, uint32_t count = 1, uint32_t pause_ms = 0);
    void stop() { _bursts_left = 0; }
    bool playing() const { return _bursts_left > 0; }

    // Adds the tone's next `count` samples into `samples`, saturating.
    void mix(int16_t* samples, size_t count);
};
