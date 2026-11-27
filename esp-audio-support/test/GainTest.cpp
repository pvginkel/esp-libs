// Gain: the volume, 0 to 1, as a gain in dB, with 0 as mute.

#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <vector>

#include "Gain.h"

namespace {

const std::vector<int16_t> SAMPLES{10000, -10000, 1, -1, INT16_MAX, INT16_MIN, 0, 12345};

std::vector<int16_t> processed(Gain& gain, std::vector<int16_t> samples) {
    gain.process(samples.data(), samples.size());
    return samples;
}

}  // namespace

TEST(Gain, OneIsTheTopOfTheRange) {
    Gain gain(1.0f);

    EXPECT_FLOAT_EQ(gain.get_db(), 0.0f);
    EXPECT_EQ(processed(gain, SAMPLES), SAMPLES);
}

TEST(Gain, ZeroMutes) {
    Gain gain(0.0f);

    EXPECT_TRUE(std::isinf(gain.get_db()) && gain.get_db() < 0.0f);
    EXPECT_EQ(processed(gain, SAMPLES), std::vector<int16_t>(SAMPLES.size(), 0));
}

TEST(Gain, JustAboveZeroIsTheBottomOfTheRange) {
    EXPECT_NEAR(Gain::volume_to_db(1e-6f, -40.0f, 0.0f), -40.0f, 1e-3f);

    Gain gain(1e-6f);

    // -40 dB is a hundredth.
    EXPECT_EQ(processed(gain, {10000, -10000, 32000}), (std::vector<int16_t>{100, -100, 320}));
}

TEST(Gain, MapsTheVolumeLinearlyInDb) {
    EXPECT_FLOAT_EQ(Gain::volume_to_db(0.5f, -40.0f, 0.0f), -20.0f);
    EXPECT_FLOAT_EQ(Gain::volume_to_db(0.25f, -40.0f, 0.0f), -30.0f);
    EXPECT_FLOAT_EQ(Gain::volume_to_db(0.5f, -30.0f, -8.0f), -19.0f);

    Gain gain(0.5f);

    // -20 dB is a tenth.
    EXPECT_EQ(processed(gain, {10000, -10000, 7}), (std::vector<int16_t>{1000, -1000, 1}));
}

TEST(Gain, ClampsTheVolume) {
    Gain gain;

    gain.set_volume(1.5f);
    EXPECT_FLOAT_EQ(gain.get_volume(), 1.0f);

    gain.set_volume(-0.5f);
    EXPECT_FLOAT_EQ(gain.get_volume(), 0.0f);

    gain.set_volume(NAN);
    EXPECT_FLOAT_EQ(gain.get_volume(), 0.0f);
}

TEST(Gain, TakesItsRangeAsAParameter) {
    Gain gain(1.0f, -30.0f, -6.0f);

    EXPECT_FLOAT_EQ(gain.get_db(), -6.0f);
    // -6 dB is about a half.
    EXPECT_EQ(processed(gain, {10000}), (std::vector<int16_t>{5012}));
}

TEST(Gain, RampsAChangeAcrossTheNextBlock) {
    Gain gain(1.0f);

    gain.set_volume(0.0f);

    const auto ramp = processed(gain, std::vector<int16_t>(100, 10000));
    EXPECT_EQ(ramp.front(), 9900);
    EXPECT_EQ(ramp[49], 5000);
    EXPECT_EQ(ramp.back(), 0);
    for (size_t i = 1; i < ramp.size(); i++) {
        EXPECT_LT(ramp[i], ramp[i - 1]) << i;
    }

    // After it, the new gain holds.
    EXPECT_EQ(processed(gain, std::vector<int16_t>(10, 10000)), std::vector<int16_t>(10, 0));
}
