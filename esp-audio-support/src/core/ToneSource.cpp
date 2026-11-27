#include "ToneSource.h"

#include <algorithm>
#include <cmath>
#include <numbers>

#include "Mix.h"

namespace {

constexpr float TWO_PI = 2.0f * std::numbers::pi_v<float>;

}  // namespace

ToneSource::ToneSource(uint32_t sample_rate_hz) : _sample_rate_hz(std::max<uint32_t>(sample_rate_hz, 1)) {}

void ToneSource::start(float frequency_hz, uint32_t duration_ms, float level, uint32_t count, uint32_t pause_ms) {
    const auto to_samples = [this](uint32_t ms) { return uint64_t{_sample_rate_hz} * ms / 1000; };

    _phase_step = TWO_PI * frequency_hz / static_cast<float>(_sample_rate_hz);
    _amplitude = std::clamp(level, 0.0f, 1.0f) * INT16_MAX;
    _burst_samples = to_samples(duration_ms);
    _pause_samples = to_samples(pause_ms);
    _fade_samples = std::min(to_samples(FADE_MS), _burst_samples / 4);
    _bursts_left = _burst_samples > 0 ? count : 0;
    _position = 0;
    _phase = 0.0f;
}

void ToneSource::mix(int16_t* samples, size_t count) {
    for (size_t i = 0; i < count && _bursts_left > 0; i++) {
        if (_position < _burst_samples) {
            auto envelope = 1.0f;
            const auto from_end = _burst_samples - _position;

            if (_position < _fade_samples) {
                envelope = static_cast<float>(_position) / static_cast<float>(_fade_samples);
            } else if (from_end <= _fade_samples) {
                envelope = static_cast<float>(from_end - 1) / static_cast<float>(_fade_samples);
            }

            mix_sample(samples[i], static_cast<int32_t>(std::lrint(_amplitude * envelope * std::sin(_phase))));

            _phase += _phase_step;
            if (_phase >= TWO_PI) {
                _phase -= TWO_PI;
            }
        }

        _position++;

        // A burst ends with its pause, except the last, which has none.
        const auto last = _bursts_left == 1;
        if (_position >= _burst_samples + (last ? 0 : _pause_samples)) {
            _bursts_left--;
            _position = 0;
            _phase = 0.0f;
        }
    }
}
