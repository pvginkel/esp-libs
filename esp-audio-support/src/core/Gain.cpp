#include "Gain.h"

#include <cmath>
#include <limits>

#include "Mix.h"

namespace {

float clamp_volume(float volume) {
    if (!(volume > 0.0f)) {
        return 0.0f;
    }
    return volume < 1.0f ? volume : 1.0f;
}

float db_to_linear(float db) { return std::isinf(db) && db < 0.0f ? 0.0f : std::pow(10.0f, db / 20.0f); }

int16_t scale(int16_t sample, float factor) {
    return saturate_sample(static_cast<int32_t>(std::lrint(static_cast<float>(sample) * factor)));
}

}  // namespace

Gain::Gain(float volume, float min_db, float max_db) : _min_db(min_db), _max_db(max_db) {
    set_volume(volume);

    _current = _target;
}

void Gain::set_volume(float volume) {
    _volume = clamp_volume(volume);
    _target = db_to_linear(volume_to_db(_volume, _min_db, _max_db));
}

float Gain::get_db() const { return volume_to_db(_volume, _min_db, _max_db); }

float Gain::volume_to_db(float volume, float min_db, float max_db) {
    volume = clamp_volume(volume);
    if (volume == 0.0f) {
        return -std::numeric_limits<float>::infinity();
    }

    return min_db + (max_db - min_db) * volume;
}

void Gain::process(int16_t* samples, size_t count) {
    if (count == 0) {
        return;
    }

    if (_current == _target) {
        if (_target == 1.0f) {
            return;
        }

        for (size_t i = 0; i < count; i++) {
            samples[i] = scale(samples[i], _target);
        }
        return;
    }

    const auto step = (_target - _current) / static_cast<float>(count);

    for (size_t i = 0; i < count; i++) {
        samples[i] = scale(samples[i], _current + step * static_cast<float>(i + 1));
    }

    _current = _target;
}
