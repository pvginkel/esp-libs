#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

// The configuration of a StreamBuffer. The streams are 16-bit mono PCM.
struct StreamBufferConfig {
    // The streams' sample rate.
    uint32_t sample_rate_hz = 48000;
    // The ring's length: the most audio the buffer holds ahead of the read position. A stream that
    // keeps ahead of real time sits on top of the lead, so the depth holds the lead, the sender's
    // lead over real time and the jitter. IntercomServer keeps 500 ms ahead, so the default leaves
    // 300 ms over the default lead.
    uint32_t depth_ms = 1000;
    // How far ahead of the read position a stream's first packet lands. At most the depth.
    uint32_t lead_ms = 200;
    // How long a stream that ran dry waits for its next packet before it ends. The player plays
    // silence meanwhile.
    uint32_t grace_ms = 500;
};

// What a StreamBuffer has counted since it was made.
struct StreamBufferStats {
    // Well-formed packets received, whatever became of them.
    uint32_t packets{};
    // Packets that never came, played as silence in their place.
    uint32_t gaps_filled{};
    // Packets that came after a later one, still in time for their place.
    uint32_t reordered{};
    // Packets dropped because their place had been played, or had been filled already.
    uint32_t late_or_duplicate{};
    // Packets dropped because they fell past the ring's end, or came from a source too many.
    uint32_t overflows{};
    // Packets dropped because they held no whole sample after the counter.
    uint32_t invalid{};
    // Streams started: a source's first packet, a jump in its counter, or a packet after its
    // stream ended.
    uint32_t new_streams{};
    // Streams that ran dry and went on within the grace.
    uint32_t underruns{};
};

// What became of a packet.
enum class PushResult {
    // In its stream, where its counter puts it.
    Placed,
    // It started a stream, the lead ahead of the read position.
    NewStream,
    // It went on with a stream that had run dry, the lead ahead of the read position: an underrun.
    Resumed,
    // It filled a gap, in time for its place.
    Reordered,
    LateOrDuplicate,
    Overflow,
    Invalid,
};

// An opaque key for a sender, which the caller picks: its address and port, for one.
using SourceId = uint64_t;

// The jitter buffer: the packets of several sources in, one mixed 16-bit mono stream out.
//
// A packet is a 4-byte big-endian counter, then 16-bit little-endian PCM. Each source has its own
// counter and its own stream:
//
// - A source's first packet starts its stream, the lead ahead of the read position.
// - Each packet lands where its counter puts it. The packet after the last one lands right after
//   it, so packets of any length play back to back. A missing packet is taken to be as long as the
//   stream's mean packet. Its place plays as silence unless it comes before that place is read.
// - A jump in the counter bigger than the depth, either way, starts a new stream instead. A sender
//   that restarts counts from 0 again, and filling that jump would play minutes of silence.
// - A stream runs dry when the read position passes its last sample. Its next packet within the
//   grace resumes it, the lead ahead, and counts an underrun. After the grace, the stream has
//   ended, and the source's next packet starts a new one.
// - The sources mix by adding, saturating at the 16-bit limits.
//
// The ring is the caller's, so the caller picks its memory. The class is not thread-safe: the
// receiver and the player run on different tasks, so the caller holds a lock around it.
class StreamBuffer {
public:
    static constexpr size_t HEADER_BYTES = 4;
    // At most this many sources play at once. A packet from one more is an overflow.
    static constexpr size_t MAX_SOURCES = 8;
    // At most this many gaps stay open per source. Past it, the oldest can no longer be filled.
    static constexpr size_t MAX_GAPS = 32;

private:
    // A run of missing packets, `count` places of `slot` samples from `position`.
    struct Gap {
        uint32_t first;
        uint32_t count;
        uint64_t position;
        uint32_t slot;
    };

    struct Source {
        SourceId id{};
        // The counter after the last packet placed in order.
        uint32_t next_counter{};
        // The position after the stream's last sample.
        uint64_t end{};
        // What the stream has placed, for its mean packet.
        uint64_t placed_samples{};
        uint32_t placed_packets{};
        // In counter order, which is also position order.
        std::vector<Gap> gaps;
    };

    int16_t* _ring;
    size_t _capacity;
    uint64_t _lead;
    uint64_t _grace;
    // Positions count samples from the buffer's start, and never wrap.
    uint64_t _read{};
    std::vector<Source> _sources;
    StreamBufferStats _stats{};

public:
    // The ring's length in samples, for the caller to allocate.
    static size_t ring_samples(const StreamBufferConfig& config);

    // `ring` holds ring_samples(config) samples and outlives the buffer.
    StreamBuffer(const StreamBufferConfig& config, int16_t* ring);

    StreamBuffer(const StreamBuffer&) = delete;
    StreamBuffer& operator=(const StreamBuffer&) = delete;

    PushResult push(SourceId source, const uint8_t* packet, size_t length);

    // The next `samples` samples of the mix, silence where no stream has any, and moves the read
    // position past them.
    void read(int16_t* out, size_t samples);

    // Whether a stream plays or waits out its grace. The player plays while this is true.
    bool playing() const { return !_sources.empty(); }

    // How far the furthest stream reaches ahead of the read position.
    size_t buffered_samples() const;

    size_t capacity() const { return _capacity; }
    uint64_t read_position() const { return _read; }
    const StreamBufferStats& stats() const { return _stats; }

    // Ends every stream and empties the ring. The stats stay.
    void reset();

private:
    Source* find(SourceId id);
    bool fits(uint64_t position, size_t samples) const { return position + samples <= _read + _capacity; }
    void start(Source& source, uint32_t counter, const uint8_t* pcm, size_t samples);
    void place(Source& source, uint64_t position, const uint8_t* pcm, size_t samples);
    void add_gap(Source& source, const Gap& gap);
    void forget_gaps(Source& source);
    PushResult fill_gap(Source& source, uint32_t counter, const uint8_t* pcm, size_t samples);
    static uint32_t mean_packet(const Source& source);
};
