// ToneSource: sine bursts, a local source.

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <vector>

#include "ToneSource.h"

namespace {

constexpr uint32_t SAMPLE_RATE_HZ = 48000;

// The sign changes, skipping zeros: two a cycle.
int zero_crossings(const std::vector<int16_t>& samples, size_t from, size_t to) {
    int crossings = 0;
    int last_sign = 0;
    for (size_t i = from; i < to; i++) {
        const auto sign = samples[i] > 0 ? 1 : samples[i] < 0 ? -1 : 0;
        if (sign != 0) {
            if (last_sign != 0 && sign != last_sign) {
                crossings++;
            }
            last_sign = sign;
        }
    }
    return crossings;
}

bool silent(const std::vector<int16_t>& samples, size_t from, size_t to) {
    return std::all_of(samples.begin() + static_cast<ptrdiff_t>(from), samples.begin() + static_cast<ptrdiff_t>(to),
                       [](int16_t sample) { return sample == 0; });
}

int16_t peak(const std::vector<int16_t>& samples, size_t from, size_t to) {
    int16_t result = 0;
    for (size_t i = from; i < to; i++) {
        result = std::max(result, static_cast<int16_t>(std::abs(samples[i])));
    }
    return result;
}

// The samples the source plays until it stops, then `after` more, mixed onto silence a sample at a
// time.
std::vector<int16_t> play(ToneSource& tone, size_t after) {
    std::vector<int16_t> samples;
    while (tone.playing()) {
        samples.push_back(0);
        tone.mix(&samples.back(), 1);
    }
    for (size_t i = 0; i < after; i++) {
        samples.push_back(0);
        tone.mix(&samples.back(), 1);
    }
    return samples;
}

}  // namespace

TEST(ToneSource, PlaysTheFrequencyForTheLength) {
    ToneSource tone(SAMPLE_RATE_HZ);

    EXPECT_FALSE(tone.playing());
    tone.start(440.0f, 100, 0.5f);
    EXPECT_TRUE(tone.playing());

    const auto samples = play(tone, 1000);

    // 100 ms at 48 kHz, then nothing.
    ASSERT_EQ(samples.size(), 4800u + 1000u);
    EXPECT_TRUE(silent(samples, 4800, samples.size()));

    // 44 cycles of 440 Hz in 100 ms.
    EXPECT_NEAR(zero_crossings(samples, 0, 4800), 88, 1);
    EXPECT_NEAR(peak(samples, 0, 4800), 16384, 20);
}

TEST(ToneSource, PlaysBurstsWithAPauseBetween) {
    ToneSource tone(SAMPLE_RATE_HZ);

    // Two 50 ms beeps of 1 kHz, 100 ms apart.
    tone.start(1000.0f, 50, 1.0f, 2, 100);

    const auto samples = play(tone, 0);

    ASSERT_EQ(samples.size(), 2400u + 4800u + 2400u);
    EXPECT_NEAR(zero_crossings(samples, 0, 2400), 100, 1);
    EXPECT_TRUE(silent(samples, 2400, 7200));
    EXPECT_NEAR(zero_crossings(samples, 7200, 9600), 100, 1);
}

TEST(ToneSource, FadesInAndOut) {
    ToneSource tone(SAMPLE_RATE_HZ);

    tone.start(1000.0f, 100, 1.0f);

    const auto samples = play(tone, 0);

    // The first and last millisecond are well under the peak.
    EXPECT_LT(peak(samples, 0, 48), 32767 / 4);
    EXPECT_LT(peak(samples, 4800 - 48, 4800), 32767 / 4);
    EXPECT_GT(peak(samples, 2400, 2448), 32000);
}

TEST(ToneSource, MixesOntoWhatIsThereWithSaturation) {
    ToneSource tone(SAMPLE_RATE_HZ);

    tone.start(1000.0f, 100, 1.0f);

    std::vector<int16_t> samples(4800, 30000);
    tone.mix(samples.data(), samples.size());

    EXPECT_EQ(*std::max_element(samples.begin(), samples.end()), INT16_MAX);
    EXPECT_LT(*std::min_element(samples.begin(), samples.end()), 0);
    EXPECT_FALSE(tone.playing());
}

TEST(ToneSource, StopEndsTheTone) {
    ToneSource tone(SAMPLE_RATE_HZ);

    tone.start(440.0f, 1000, 0.5f);
    tone.stop();

    std::vector<int16_t> samples(100, 0);
    tone.mix(samples.data(), samples.size());

    EXPECT_FALSE(tone.playing());
    EXPECT_TRUE(silent(samples, 0, samples.size()));
}
