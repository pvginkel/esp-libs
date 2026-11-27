#pragma once

#include <algorithm>
#include <cstdint>

// A sum of 16-bit samples, held at the 16-bit limits instead of wrapping round.
inline int16_t saturate_sample(int32_t value) {
    return static_cast<int16_t>(std::clamp<int32_t>(value, INT16_MIN, INT16_MAX));
}

// Adds `sample` into `target`, saturating.
inline void mix_sample(int16_t& target, int32_t sample) { target = saturate_sample(target + sample); }
