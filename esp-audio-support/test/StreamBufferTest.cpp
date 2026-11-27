// StreamBuffer: the packets of several sources in, one mixed stream out.

#include <gtest/gtest.h>

#include <cstdint>
#include <vector>

#include "StreamBuffer.h"

namespace {

constexpr uint32_t SAMPLE_RATE_HZ = 48000;
// IntercomServer's payload: 1468 bytes.
constexpr size_t PACKET = 734;
// The default lead, 200 ms, and the default grace, 500 ms.
constexpr size_t LEAD = 9600;
constexpr size_t GRACE = 24000;
constexpr SourceId SERVER = 1;
constexpr SourceId OTHER = 2;

std::vector<uint8_t> make_packet(uint32_t counter, const std::vector<int16_t>& samples) {
    std::vector<uint8_t> packet{
        static_cast<uint8_t>(counter >> 24),
        static_cast<uint8_t>(counter >> 16),
        static_cast<uint8_t>(counter >> 8),
        static_cast<uint8_t>(counter),
    };
    for (const auto sample : samples) {
        const auto bits = static_cast<uint16_t>(sample);
        packet.push_back(static_cast<uint8_t>(bits));
        packet.push_back(static_cast<uint8_t>(bits >> 8));
    }
    return packet;
}

// The audio a test expects, a run of one value at a time.
struct Audio {
    std::vector<int16_t> samples;

    Audio& add(size_t count, int16_t value) {
        samples.insert(samples.end(), count, value);
        return *this;
    }
    Audio& silence(size_t count) { return add(count, 0); }
};

// A buffer on the default config, which is 48 kHz, 1 s deep, with a 200 ms lead and a 500 ms
// grace.
struct Buffer {
    StreamBufferConfig config;
    std::vector<int16_t> ring;
    StreamBuffer buffer;

    explicit Buffer(StreamBufferConfig config = {})
        : config(config), ring(StreamBuffer::ring_samples(config)), buffer(config, ring.data()) {}

    // A packet of `count` samples of one value.
    PushResult push(uint32_t counter, int16_t value, size_t count = PACKET, SourceId source = SERVER) {
        const auto packet = make_packet(counter, std::vector<int16_t>(count, value));
        return buffer.push(source, packet.data(), packet.size());
    }

    std::vector<int16_t> read(size_t count) {
        std::vector<int16_t> out(count, 12345);
        buffer.read(out.data(), count);
        return out;
    }

    const StreamBufferStats& stats() const { return buffer.stats(); }
};

// The packet value for a counter, so each packet can be told apart.
int16_t value_of(uint32_t counter) { return static_cast<int16_t>(100 + counter); }

}  // namespace

TEST(StreamBuffer, SizesTheRingFromTheDepth) {
    EXPECT_EQ(StreamBuffer::ring_samples({}), 48000u);
    EXPECT_EQ(StreamBuffer::ring_samples({.sample_rate_hz = 16000, .depth_ms = 500}), 8000u);
}

TEST(StreamBuffer, PlaysPacketsInOrderTheLeadAhead) {
    Buffer b;

    EXPECT_EQ(b.push(0, value_of(0)), PushResult::NewStream);
    for (uint32_t counter = 1; counter < 5; counter++) {
        EXPECT_EQ(b.push(counter, value_of(counter)), PushResult::Placed);
    }

    EXPECT_TRUE(b.buffer.playing());
    EXPECT_EQ(b.buffer.buffered_samples(), LEAD + 5 * PACKET);

    Audio expected;
    expected.silence(LEAD);
    for (uint32_t counter = 0; counter < 5; counter++) {
        expected.add(PACKET, value_of(counter));
    }
    expected.silence(100);

    EXPECT_EQ(b.read(expected.samples.size()), expected.samples);

    EXPECT_EQ(b.stats().packets, 5u);
    EXPECT_EQ(b.stats().new_streams, 1u);
    EXPECT_EQ(b.stats().gaps_filled, 0u);
    EXPECT_EQ(b.stats().reordered, 0u);
    EXPECT_EQ(b.stats().late_or_duplicate, 0u);
    EXPECT_EQ(b.stats().underruns, 0u);
    EXPECT_EQ(b.stats().overflows, 0u);
}

TEST(StreamBuffer, AGapPlaysAsSilenceOfItsLength) {
    Buffer b;

    b.push(0, value_of(0));
    b.push(1, value_of(1));
    EXPECT_EQ(b.push(3, value_of(3)), PushResult::Placed);
    EXPECT_EQ(b.push(7, value_of(7)), PushResult::Placed);

    // Nothing is filled until its place is read.
    EXPECT_EQ(b.stats().gaps_filled, 0u);

    const auto expected = Audio()
                              .silence(LEAD)
                              .add(PACKET, value_of(0))
                              .add(PACKET, value_of(1))
                              .silence(PACKET)
                              .add(PACKET, value_of(3))
                              .silence(3 * PACKET)
                              .add(PACKET, value_of(7))
                              .samples;

    EXPECT_EQ(b.read(expected.size()), expected);

    EXPECT_EQ(b.stats().packets, 4u);
    EXPECT_EQ(b.stats().gaps_filled, 4u);
    EXPECT_EQ(b.stats().late_or_duplicate, 0u);
}

TEST(StreamBuffer, CountsAGapFilledAsItsPlaceIsRead) {
    Buffer b;

    b.push(0, value_of(0));
    b.push(4, value_of(4));

    b.read(LEAD + PACKET);
    EXPECT_EQ(b.stats().gaps_filled, 0u);

    // Into the first missing packet's place, and then the second's.
    b.read(1);
    EXPECT_EQ(b.stats().gaps_filled, 1u);
    b.read(PACKET);
    EXPECT_EQ(b.stats().gaps_filled, 2u);
    b.read(2 * PACKET);
    EXPECT_EQ(b.stats().gaps_filled, 3u);
}

TEST(StreamBuffer, PlacesPacketsReorderedWithinTheDepth) {
    Buffer b;

    for (const uint32_t counter : {0u, 2u, 1u, 5u, 4u, 3u, 6u}) {
        b.push(counter, value_of(counter));
    }

    Audio expected;
    expected.silence(LEAD);
    for (uint32_t counter = 0; counter < 7; counter++) {
        expected.add(PACKET, value_of(counter));
    }

    EXPECT_EQ(b.read(expected.samples.size()), expected.samples);

    EXPECT_EQ(b.stats().packets, 7u);
    EXPECT_EQ(b.stats().reordered, 3u);
    EXPECT_EQ(b.stats().gaps_filled, 0u);
    EXPECT_EQ(b.stats().late_or_duplicate, 0u);
    EXPECT_EQ(b.stats().new_streams, 1u);
}

TEST(StreamBuffer, DropsADuplicate) {
    Buffer b;

    b.push(0, value_of(0));
    b.push(1, value_of(1));
    EXPECT_EQ(b.push(1, value_of(1)), PushResult::LateOrDuplicate);
    b.push(3, value_of(3));
    EXPECT_EQ(b.push(2, value_of(2)), PushResult::Reordered);
    EXPECT_EQ(b.push(2, value_of(2)), PushResult::LateOrDuplicate);
    EXPECT_EQ(b.push(3, value_of(3)), PushResult::LateOrDuplicate);

    // Each plays once, not mixed onto itself.
    Audio expected;
    expected.silence(LEAD);
    for (uint32_t counter = 0; counter < 4; counter++) {
        expected.add(PACKET, value_of(counter));
    }

    EXPECT_EQ(b.read(expected.samples.size()), expected.samples);

    EXPECT_EQ(b.stats().packets, 7u);
    EXPECT_EQ(b.stats().late_or_duplicate, 3u);
    EXPECT_EQ(b.stats().reordered, 1u);
    EXPECT_EQ(b.stats().gaps_filled, 0u);
}

TEST(StreamBuffer, DropsAPacketWhosePlaceHasPlayed) {
    Buffer b;

    b.push(0, value_of(0));
    b.push(2, value_of(2));

    // One sample into packet 1's place.
    b.read(LEAD + PACKET + 1);

    EXPECT_EQ(b.push(1, value_of(1)), PushResult::LateOrDuplicate);

    const auto expected = Audio().silence(PACKET - 1).add(PACKET, value_of(2)).samples;
    EXPECT_EQ(b.read(expected.size()), expected);

    EXPECT_EQ(b.stats().late_or_duplicate, 1u);
    EXPECT_EQ(b.stats().gaps_filled, 1u);
}

TEST(StreamBuffer, AForwardJumpBiggerThanTheDepthStartsANewStream) {
    Buffer b;

    b.push(0, value_of(0));
    b.push(1, value_of(1));
    b.read(LEAD + PACKET);

    // 66 missing packets of 734 samples are more than the depth's 48000.
    EXPECT_EQ(b.push(2 + 66, value_of(68)), PushResult::NewStream);
    EXPECT_EQ(b.push(2 + 67, value_of(69)), PushResult::Placed);

    const auto expected = Audio()
                              .add(PACKET, value_of(1))
                              .silence(LEAD - PACKET)
                              .add(PACKET, value_of(68))
                              .add(PACKET, value_of(69))
                              .samples;
    EXPECT_EQ(b.read(expected.size()), expected);

    EXPECT_EQ(b.stats().new_streams, 2u);
    EXPECT_EQ(b.stats().gaps_filled, 0u);
    EXPECT_EQ(b.stats().underruns, 0u);
}

TEST(StreamBuffer, AForwardJumpWithinTheDepthIsAGap) {
    Buffer b;

    b.push(0, value_of(0));

    // 50 missing packets of 734 samples fit the depth's 48000, after the lead.
    EXPECT_EQ(b.push(1 + 50, value_of(51)), PushResult::Placed);

    const auto expected = Audio().silence(LEAD).add(PACKET, value_of(0)).silence(50 * PACKET).add(PACKET, value_of(51));
    EXPECT_EQ(b.read(expected.samples.size()), expected.samples);

    EXPECT_EQ(b.stats().new_streams, 1u);
    EXPECT_EQ(b.stats().gaps_filled, 50u);
}

TEST(StreamBuffer, ABackwardJumpBiggerThanTheDepthStartsANewStream) {
    Buffer b;

    // IntercomServer restarts, and its counter with it.
    b.push(5000, value_of(1));
    b.push(5001, value_of(2));
    b.read(LEAD + PACKET);

    EXPECT_EQ(b.push(0, value_of(10)), PushResult::NewStream);
    EXPECT_EQ(b.push(1, value_of(11)), PushResult::Placed);

    const auto expected = Audio()
                              .add(PACKET, value_of(2))
                              .silence(LEAD - PACKET)
                              .add(PACKET, value_of(10))
                              .add(PACKET, value_of(11))
                              .samples;
    EXPECT_EQ(b.read(expected.size()), expected);

    EXPECT_EQ(b.stats().new_streams, 2u);
    EXPECT_EQ(b.stats().late_or_duplicate, 0u);
}

TEST(StreamBuffer, ABackwardStepWithinTheDepthIsLate) {
    Buffer b;

    for (uint32_t counter = 100; counter < 105; counter++) {
        b.push(counter, value_of(counter));
    }

    // 55 packets back is 40370 samples, within the depth.
    EXPECT_EQ(b.push(50, value_of(50)), PushResult::LateOrDuplicate);

    EXPECT_EQ(b.stats().new_streams, 1u);
    EXPECT_EQ(b.stats().late_or_duplicate, 1u);
}

TEST(StreamBuffer, TheStreamEndsWhenItRunsDry) {
    Buffer b;

    b.push(0, value_of(0));
    b.push(1, value_of(1));
    b.push(2, value_of(2));

    b.read(LEAD + 3 * PACKET);
    EXPECT_EQ(b.buffer.buffered_samples(), 0u);

    // Dry, it waits out the grace in silence.
    EXPECT_TRUE(b.buffer.playing());
    EXPECT_EQ(b.read(GRACE - 1), std::vector<int16_t>(GRACE - 1, 0));
    EXPECT_TRUE(b.buffer.playing());

    b.read(1);
    EXPECT_FALSE(b.buffer.playing());

    // Its next packet, even with the next counter, starts a new stream.
    EXPECT_EQ(b.push(3, value_of(3)), PushResult::NewStream);

    const auto expected = Audio().silence(LEAD).add(PACKET, value_of(3)).samples;
    EXPECT_EQ(b.read(expected.size()), expected);

    EXPECT_EQ(b.stats().new_streams, 2u);
    EXPECT_EQ(b.stats().underruns, 0u);
}

TEST(StreamBuffer, APacketWithinTheGraceResumesTheStreamAsAnUnderrun) {
    Buffer b;

    b.push(0, value_of(0));
    b.push(1, value_of(1));
    b.read(LEAD + 2 * PACKET + 1000);

    // Packet 2 was lost in the stall as well.
    EXPECT_EQ(b.push(3, value_of(3)), PushResult::Resumed);
    EXPECT_EQ(b.push(4, value_of(4)), PushResult::Placed);
    EXPECT_EQ(b.push(2, value_of(2)), PushResult::LateOrDuplicate);

    const auto expected = Audio().silence(LEAD).add(PACKET, value_of(3)).add(PACKET, value_of(4)).samples;
    EXPECT_EQ(b.read(expected.size()), expected);

    EXPECT_EQ(b.stats().underruns, 1u);
    EXPECT_EQ(b.stats().new_streams, 1u);
    EXPECT_EQ(b.stats().late_or_duplicate, 1u);
}

TEST(StreamBuffer, TwoSourcesMixWithSaturation) {
    Buffer b;

    // Each has its own counter.
    EXPECT_EQ(b.push(0, 20000, PACKET, SERVER), PushResult::NewStream);
    EXPECT_EQ(b.push(700, 20000, PACKET, OTHER), PushResult::NewStream);
    EXPECT_EQ(b.push(1, -20000, PACKET, SERVER), PushResult::Placed);
    EXPECT_EQ(b.push(701, -20000, PACKET, OTHER), PushResult::Placed);
    EXPECT_EQ(b.push(2, 1000, PACKET, SERVER), PushResult::Placed);
    EXPECT_EQ(b.push(702, 2000, PACKET, OTHER), PushResult::Placed);
    EXPECT_EQ(b.push(3, -3000, PACKET, SERVER), PushResult::Placed);

    const auto expected = Audio()
                              .silence(LEAD)
                              .add(PACKET, INT16_MAX)
                              .add(PACKET, INT16_MIN)
                              .add(PACKET, 3000)
                              .add(PACKET, -3000)
                              .samples;
    EXPECT_EQ(b.read(expected.size()), expected);

    EXPECT_EQ(b.stats().packets, 7u);
    EXPECT_EQ(b.stats().new_streams, 2u);
}

TEST(StreamBuffer, PlaysPacketsOfAnyLengthBackToBack) {
    Buffer b;

    // A 20 ms chunk at 48 kHz, 1920 bytes, goes out as 1468 bytes and then 452.
    b.push(0, 1, 734);
    b.push(1, 2, 226);
    b.push(2, 3, 734);
    b.push(3, 4, 226);

    const auto expected = Audio().silence(LEAD).add(734, 1).add(226, 2).add(734, 3).add(226, 4).silence(10).samples;
    EXPECT_EQ(b.read(expected.size()), expected);
}

TEST(StreamBuffer, TakesAMissingPacketToBeTheMeanPacket) {
    Buffer b;

    b.push(0, 1, 734);
    b.push(1, 2, 226);
    b.push(3, 4, 226);

    const auto expected = Audio().silence(LEAD).add(734, 1).add(226, 2).silence(480).add(226, 4).samples;
    EXPECT_EQ(b.read(expected.size()), expected);

    EXPECT_EQ(b.stats().gaps_filled, 1u);
}

TEST(StreamBuffer, ACounterThatWrapsGoesOn) {
    Buffer b;

    b.push(0xFFFFFFFE, 1);
    EXPECT_EQ(b.push(0xFFFFFFFF, 2), PushResult::Placed);
    EXPECT_EQ(b.push(0, 3), PushResult::Placed);
    EXPECT_EQ(b.push(1, 4), PushResult::Placed);

    const auto expected = Audio().silence(LEAD).add(PACKET, 1).add(PACKET, 2).add(PACKET, 3).add(PACKET, 4).samples;
    EXPECT_EQ(b.read(expected.size()), expected);

    EXPECT_EQ(b.stats().new_streams, 1u);
}

TEST(StreamBuffer, DropsAPacketPastTheRingsEnd) {
    Buffer b;

    // 52 packets fit between the lead and the depth's end.
    for (uint32_t counter = 0; counter < 52; counter++) {
        ASSERT_NE(b.push(counter, 1), PushResult::Overflow) << counter;
    }
    EXPECT_EQ(b.push(52, 1), PushResult::Overflow);
    EXPECT_EQ(b.buffer.buffered_samples(), LEAD + 52 * PACKET);

    // Once a packet's length has played, it fits.
    b.read(PACKET);
    EXPECT_EQ(b.push(52, 1), PushResult::Placed);

    EXPECT_EQ(b.stats().packets, 54u);
    EXPECT_EQ(b.stats().overflows, 1u);
}

TEST(StreamBuffer, DropsASourceTooMany) {
    Buffer b;

    for (SourceId source = 0; source < StreamBuffer::MAX_SOURCES; source++) {
        EXPECT_EQ(b.push(0, 1, PACKET, source), PushResult::NewStream);
    }
    EXPECT_EQ(b.push(0, 1, PACKET, StreamBuffer::MAX_SOURCES), PushResult::Overflow);

    EXPECT_EQ(b.stats().new_streams, StreamBuffer::MAX_SOURCES);
    EXPECT_EQ(b.stats().overflows, 1u);
}

TEST(StreamBuffer, DropsAPacketWithNoWholeSample) {
    Buffer b;

    const uint8_t packet[] = {0, 0, 0, 1, 0x10, 0x20, 0x30};

    EXPECT_EQ(b.buffer.push(SERVER, packet, 3), PushResult::Invalid);
    EXPECT_EQ(b.buffer.push(SERVER, packet, 4), PushResult::Invalid);
    EXPECT_EQ(b.buffer.push(SERVER, packet, 5), PushResult::Invalid);
    EXPECT_EQ(b.buffer.push(SERVER, packet, 7), PushResult::Invalid);

    EXPECT_FALSE(b.buffer.playing());
    EXPECT_EQ(b.stats().invalid, 4u);
    EXPECT_EQ(b.stats().packets, 0u);
}

TEST(StreamBuffer, ReadsLittleEndianSamples) {
    Buffer b;

    const auto packet = make_packet(0, {0x1234, -2, INT16_MIN});
    b.buffer.push(SERVER, packet.data(), packet.size());

    b.read(LEAD);
    EXPECT_EQ(b.read(3), (std::vector<int16_t>{0x1234, -2, INT16_MIN}));
}

TEST(StreamBuffer, ResetEndsEveryStream) {
    Buffer b;

    b.push(0, 1);
    b.push(1, 2);
    b.buffer.reset();

    EXPECT_FALSE(b.buffer.playing());
    EXPECT_EQ(b.read(LEAD + 2 * PACKET), std::vector<int16_t>(LEAD + 2 * PACKET, 0));

    EXPECT_EQ(b.push(2, 3), PushResult::NewStream);
    EXPECT_EQ(b.stats().new_streams, 2u);
}

TEST(StreamBuffer, TakesTheSampleRateAndDepthAsParameters) {
    // Intercom's: 16 kHz, a 200 ms lead in a 400 ms ring.
    Buffer b({.sample_rate_hz = 16000, .depth_ms = 400, .lead_ms = 200, .grace_ms = 100});

    EXPECT_EQ(b.buffer.capacity(), 6400u);

    b.push(0, 1, 320);
    b.push(1, 2, 320);

    const auto expected = Audio().silence(3200).add(320, 1).add(320, 2).samples;
    EXPECT_EQ(b.read(expected.size()), expected);

    // A 100 ms grace at 16 kHz.
    b.read(1599);
    EXPECT_TRUE(b.buffer.playing());
    b.read(1);
    EXPECT_FALSE(b.buffer.playing());
}
