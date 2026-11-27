#include "AutoVolume.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "Mix.h"

AutoVolume::AutoVolume(uint32_t sample_rate_hz, float target_db)
    : _sample_rate_hz(static_cast<float>(sample_rate_hz)),
      _target_db(target_db),
      _limiter(std::pow(10.0f, LIMITER_DB / 20.0f) * INT16_MAX) {}

void AutoVolume::reset() {
    _envelope = 0.0f;
    _gain_db = 0.0f;
    _gain = 1.0f;
    _hold = 0.0f;
}

void AutoVolume::process(int16_t* samples, size_t count) {
    if (count == 0) {
        return;
    }

    // The block's level.
    float sum_of_squares = 0.0f;
    for (size_t i = 0; i < count; i++) {
        const auto sample = static_cast<float>(samples[i]) / INT16_MAX;
        sum_of_squares += sample * sample;
    }
    const auto rms = std::sqrt(sum_of_squares / static_cast<float>(count));

    // The envelope, one pole at the block rate.
    const auto block_s = static_cast<float>(count) / _sample_rate_hz;
    const auto envelope_alpha = std::exp(-block_s / WINDOW_S);
    _envelope = envelope_alpha * _envelope + (1.0f - envelope_alpha) * rms;

    // The attenuation the envelope asks for, at most 0, through the soft knee.
    const auto envelope_db = 20.0f * std::log10(_envelope + std::numeric_limits<float>::min());
    const auto over_db = envelope_db - _target_db;

    float wanted_db = 0.0f;
    if (over_db >= KNEE_DB / 2) {
        wanted_db = -over_db;
    } else if (over_db > -KNEE_DB / 2) {
        const auto x = over_db + KNEE_DB / 2;
        wanted_db = -(x * x) / (2.0f * KNEE_DB);
    }

    // Attack, hold and release.
    const auto attack = wanted_db < _gain_db;
    if (attack) {
        _hold = HOLD_S;
    } else if (_hold > 0.0f) {
        _hold = std::max(0.0f, _hold - block_s);
    }

    if (attack || _hold <= 0.0f) {
        const auto alpha = std::exp(-block_s / (attack ? ATTACK_S : RELEASE_S));
        _gain_db = alpha * _gain_db + (1.0f - alpha) * wanted_db;
    }

    _gain_db = std::clamp(_gain_db, -MAX_ATTENUATION_DB, 0.0f);

    // The gain ramps from the last block's to this one's, then the limiter.
    const auto from = _gain;
    const auto to = std::pow(10.0f, _gain_db / 20.0f);
    const auto step = (to - from) / static_cast<float>(count);

    for (size_t i = 0; i < count; i++) {
        const auto gain = from + step * static_cast<float>(i + 1);
        const auto wet = std::clamp(static_cast<float>(samples[i]) * gain, -_limiter, _limiter);

        samples[i] = saturate_sample(static_cast<int32_t>(std::lrint(wet)));
    }

    _gain = to;
}
