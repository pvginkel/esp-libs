#include "StreamBuffer.h"

#include <algorithm>

#include "Mix.h"

namespace {

uint64_t ms_to_samples(uint32_t sample_rate_hz, uint32_t ms) { return uint64_t{sample_rate_hz} * ms / 1000; }

uint32_t read_counter(const uint8_t* packet) {
    return uint32_t{packet[0]} << 24 | uint32_t{packet[1]} << 16 | uint32_t{packet[2]} << 8 | uint32_t{packet[3]};
}

int16_t read_sample(const uint8_t* pcm, size_t index) {
    return static_cast<int16_t>(static_cast<uint16_t>(pcm[index * 2] | pcm[index * 2 + 1] << 8));
}

}  // namespace

size_t StreamBuffer::ring_samples(const StreamBufferConfig& config) {
    return static_cast<size_t>(std::max<uint64_t>(ms_to_samples(config.sample_rate_hz, config.depth_ms), 1));
}

StreamBuffer::StreamBuffer(const StreamBufferConfig& config, int16_t* ring)
    : _ring(ring),
      _capacity(ring_samples(config)),
      _lead(std::min<uint64_t>(ms_to_samples(config.sample_rate_hz, config.lead_ms), _capacity)),
      _grace(ms_to_samples(config.sample_rate_hz, config.grace_ms)) {
    _sources.reserve(MAX_SOURCES);

    std::fill(_ring, _ring + _capacity, int16_t{0});
}

PushResult StreamBuffer::push(SourceId id, const uint8_t* packet, size_t length) {
    if (length < HEADER_BYTES + 2 || (length - HEADER_BYTES) % 2 != 0) {
        _stats.invalid++;
        return PushResult::Invalid;
    }

    _stats.packets++;

    const auto counter = read_counter(packet);
    const auto pcm = packet + HEADER_BYTES;
    const auto samples = (length - HEADER_BYTES) / 2;

    auto source = find(id);

    if (!source) {
        if (_sources.size() >= MAX_SOURCES || !fits(_read + _lead, samples)) {
            _stats.overflows++;
            return PushResult::Overflow;
        }

        auto& added = _sources.emplace_back();
        added.id = id;
        start(added, counter, pcm, samples);
        _stats.new_streams++;
        return PushResult::NewStream;
    }

    // How far the counter is ahead of the stream's next, in either direction, and how much audio
    // that jump spans.
    const auto ahead = static_cast<int32_t>(counter - source->next_counter);
    const auto jump = static_cast<uint32_t>(ahead < 0 ? -static_cast<int64_t>(ahead) : ahead);
    const auto jump_samples = uint64_t{jump} * mean_packet(*source);

    // A packet that would not fit the lead ahead leaves the stream as it is.
    const auto restart = [&](PushResult result) {
        if (!fits(_read + _lead, samples)) {
            _stats.overflows++;
            return PushResult::Overflow;
        }

        start(*source, counter, pcm, samples);

        if (result == PushResult::Resumed) {
            _stats.underruns++;
        } else {
            _stats.new_streams++;
        }
        return result;
    };

    if (source->end <= _read) {
        // The stream ran dry and waits out its grace: whatever follows on resumes it.
        if (ahead >= 0) {
            return restart(PushResult::Resumed);
        }
        if (jump_samples > _capacity) {
            return restart(PushResult::NewStream);
        }

        _stats.late_or_duplicate++;
        return PushResult::LateOrDuplicate;
    }

    if (jump_samples > _capacity) {
        return restart(PushResult::NewStream);
    }

    if (ahead < 0) {
        return fill_gap(*source, counter, pcm, samples);
    }

    const auto slot = mean_packet(*source);
    const auto position = source->end + uint64_t{jump} * slot;

    if (!fits(position, samples)) {
        _stats.overflows++;
        return PushResult::Overflow;
    }

    if (jump > 0) {
        add_gap(*source, Gap{.first = source->next_counter, .count = jump, .position = source->end, .slot = slot});
    }

    place(*source, position, pcm, samples);
    source->end = position + samples;
    source->next_counter = counter + 1;

    return PushResult::Placed;
}

void StreamBuffer::read(int16_t* out, size_t samples) {
    // Past the ring's length the ring holds nothing, so a longer read ends in silence.
    for (size_t done = 0; done < samples;) {
        const auto index = static_cast<size_t>((_read + done) % _capacity);
        const auto chunk = std::min(samples - done, _capacity - index);

        std::copy(_ring + index, _ring + index + chunk, out + done);
        std::fill(_ring + index, _ring + index + chunk, int16_t{0});

        done += chunk;
    }

    _read += samples;

    // A gap's place, once its first sample is read, has played as silence.
    for (auto& source : _sources) {
        while (!source.gaps.empty()) {
            auto& gap = source.gaps.front();
            if (gap.position >= _read) {
                break;
            }

            const auto passed =
                static_cast<uint32_t>(std::min<uint64_t>(gap.count, (_read - gap.position + gap.slot - 1) / gap.slot));

            _stats.gaps_filled += passed;

            gap.first += passed;
            gap.count -= passed;
            gap.position += uint64_t{passed} * gap.slot;

            if (gap.count > 0) {
                break;
            }

            source.gaps.erase(source.gaps.begin());
        }
    }

    std::erase_if(_sources,
                  [this](const Source& source) { return source.end <= _read && _read - source.end >= _grace; });
}

size_t StreamBuffer::buffered_samples() const {
    uint64_t result = 0;

    for (const auto& source : _sources) {
        if (source.end > _read) {
            result = std::max(result, source.end - _read);
        }
    }

    return static_cast<size_t>(result);
}

void StreamBuffer::reset() {
    _sources.clear();

    std::fill(_ring, _ring + _capacity, int16_t{0});
}

StreamBuffer::Source* StreamBuffer::find(SourceId id) {
    const auto it = std::find_if(_sources.begin(), _sources.end(), [id](const Source& s) { return s.id == id; });

    return it == _sources.end() ? nullptr : &*it;
}

void StreamBuffer::start(Source& source, uint32_t counter, const uint8_t* pcm, size_t samples) {
    // A stream that jumps still has its tail ahead of the read position: it plays out, mixed with
    // the new stream.
    forget_gaps(source);

    source.placed_samples = 0;
    source.placed_packets = 0;

    const auto position = _read + _lead;

    place(source, position, pcm, samples);
    source.end = position + samples;
    source.next_counter = counter + 1;
}

void StreamBuffer::place(Source& source, uint64_t position, const uint8_t* pcm, size_t samples) {
    for (size_t done = 0; done < samples;) {
        const auto index = static_cast<size_t>((position + done) % _capacity);
        const auto chunk = std::min(samples - done, _capacity - index);

        for (size_t i = 0; i < chunk; i++) {
            mix_sample(_ring[index + i], read_sample(pcm, done + i));
        }

        done += chunk;
    }

    source.placed_samples += samples;
    source.placed_packets++;
}

void StreamBuffer::add_gap(Source& source, const Gap& gap) {
    if (source.gaps.size() >= MAX_GAPS) {
        // The oldest gap plays as silence, but a packet that comes for it is no longer placed.
        _stats.gaps_filled += source.gaps.front().count;
        source.gaps.erase(source.gaps.begin());
    }

    source.gaps.push_back(gap);
}

void StreamBuffer::forget_gaps(Source& source) {
    for (const auto& gap : source.gaps) {
        _stats.gaps_filled += gap.count;
    }

    source.gaps.clear();
}

PushResult StreamBuffer::fill_gap(Source& source, uint32_t counter, const uint8_t* pcm, size_t samples) {
    const auto it = std::find_if(source.gaps.begin(), source.gaps.end(),
                                 [counter](const Gap& gap) { return counter - gap.first < gap.count; });

    if (it == source.gaps.end()) {
        _stats.late_or_duplicate++;
        return PushResult::LateOrDuplicate;
    }

    const auto gap = *it;
    const auto index = counter - gap.first;
    const auto position = gap.position + uint64_t{index} * gap.slot;

    // The packet keeps to its place, so a longer one does not run into the next.
    place(source, position, pcm, std::min<size_t>(samples, gap.slot));

    // The gap splits around the place it filled.
    const Gap before{.first = gap.first, .count = index, .position = gap.position, .slot = gap.slot};
    const Gap after{
        .first = counter + 1,
        .count = gap.count - index - 1,
        .position = position + gap.slot,
        .slot = gap.slot,
    };

    auto next = source.gaps.erase(it);
    if (after.count > 0) {
        next = source.gaps.insert(next, after);
    }
    if (before.count > 0) {
        source.gaps.insert(next, before);
    }

    _stats.reordered++;
    return PushResult::Reordered;
}

uint32_t StreamBuffer::mean_packet(const Source& source) {
    if (source.placed_packets == 0) {
        return 1;
    }

    return static_cast<uint32_t>((source.placed_samples + source.placed_packets / 2) / source.placed_packets);
}
