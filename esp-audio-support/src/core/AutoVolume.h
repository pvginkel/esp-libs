#pragma once

#include <cstddef>
#include <cstdint>

// An optional stage: automatic volume for one channel, downward only. It holds the level, an RMS
// envelope over the blocks, at or below a target in dBFS:
//
// - a soft knee 6 dB wide around the target;
// - a fast attack, then a hold that keeps the gain from chattering between syllables, then a slow
//   release;
// - at most 24 dB of attenuation, and never any gain;
// - a hard limit at -0.1 dBFS that catches what the gain lets through.
//
// The gain is worked out once a block, from the block's level, and ramps across the block sample by
// sample, so a step in it does not click. The volume is a separate stage (Gain), so a change of the
// volume leaves this stage's state alone.
class AutoVolume {
public:
    static constexpr float ATTACK_S = 0.010f;
    static constexpr float RELEASE_S = 0.150f;
    // The RMS envelope's time constant.
    static constexpr float WINDOW_S = 0.020f;
    static constexpr float HOLD_S = 0.080f;
    static constexpr float KNEE_DB = 6.0f;
    static constexpr float MAX_ATTENUATION_DB = 24.0f;
    static constexpr float LIMITER_DB = -0.1f;

private:
    float _sample_rate_hz;
    float _target_db;
    float _limiter;
    // The RMS envelope, linear and full scale at 1.
    float _envelope{};
    // The attenuation in dB, at most 0, and its linear factor at the end of the last block.
    float _gain_db{};
    float _gain{1.0f};
    // Seconds left of the hold.
    float _hold{};

public:
    explicit AutoVolume(uint32_t sample_rate_hz, float target_db = -18.0f);

    // Forgets the envelope and the gain, for the start of a stream.
    void reset();

    void process(int16_t* samples, size_t count);

    float get_target_db() const { return _target_db; }
    void set_target_db(float target_db) { _target_db = target_db; }

    // The attenuation in dB the last block ended on.
    float get_gain_db() const { return _gain_db; }
};
