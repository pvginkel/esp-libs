// AutoVolume: downward-only automatic volume, an optional stage.

#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <numbers>
#include <vector>

#include "AutoVolume.h"

namespace {

constexpr uint32_t SAMPLE_RATE_HZ = 48000;
// 20 ms blocks.
constexpr size_t BLOCK = 960;

// A 1 kHz sine with a peak of `peak`, one block of it at a time.
struct Sine {
    float peak;
    size_t position{};

    std::vector<int16_t> next() {
        std::vector<int16_t> block(BLOCK);
        for (auto& sample : block) {
            const auto phase = 2.0 * std::numbers::pi * 1000.0 * static_cast<double>(position++) / SAMPLE_RATE_HZ;
            sample = static_cast<int16_t>(std::lrint(peak * std::sin(phase)));
        }
        return block;
    }
};

double rms_db(const std::vector<int16_t>& block) {
    double sum = 0;
    for (const auto sample : block) {
        sum += (sample / 32767.0) * (sample / 32767.0);
    }
    return 20.0 * std::log10(std::sqrt(sum / static_cast<double>(block.size())));
}

}  // namespace

TEST(AutoVolume, LeavesAQuietSignalAlone) {
    AutoVolume volume(SAMPLE_RATE_HZ, -18.0f);
    // About -33 dBFS RMS, well under the knee.
    Sine sine{.peak = 1000.0f};

    for (int i = 0; i < 50; i++) {
        const auto block = sine.next();
        auto processed = block;
        volume.process(processed.data(), processed.size());

        ASSERT_EQ(processed, block) << "block " << i;
    }

    EXPECT_FLOAT_EQ(volume.get_gain_db(), 0.0f);
}

TEST(AutoVolume, HoldsALoudSignalAtTheTarget) {
    AutoVolume volume(SAMPLE_RATE_HZ, -18.0f);
    // A full-scale sine is -3 dBFS RMS.
    Sine sine{.peak = 32000.0f};

    std::vector<int16_t> block;
    for (int i = 0; i < 50; i++) {
        block = sine.next();
        volume.process(block.data(), block.size());
    }

    EXPECT_NEAR(rms_db(block), -18.0, 0.5);
    EXPECT_NEAR(volume.get_gain_db(), -15.0f, 0.5f);
}

TEST(AutoVolume, AttenuatesNoMoreThanItsMaximum) {
    AutoVolume volume(SAMPLE_RATE_HZ, -60.0f);
    Sine sine{.peak = 32000.0f};

    for (int i = 0; i < 50; i++) {
        auto block = sine.next();
        volume.process(block.data(), block.size());
    }

    EXPECT_FLOAT_EQ(volume.get_gain_db(), -AutoVolume::MAX_ATTENUATION_DB);
}
