# Audio playback support for the ESP32

A stream of 16-bit mono PCM over UDP, buffered against the network's jitter and mixed from several
sources. This much is the pure core in `src/core/`: it includes the C++ standard library only, and
`test/` builds it natively.

- `StreamBuffer`: the jitter buffer. Each packet is a 4-byte big-endian counter, then 16-bit
  little-endian PCM. It places each packet by its counter and plays a lost one as silence, and it
  mixes the streams of several sources, each keyed by an opaque `SourceId` the caller picks. It
  counts what it does in `stats()`.
- `Gain`: the volume, from 0 to 1, as a gain in dB. 0 mutes.
- `AutoVolume`: an optional stage that holds the level at or below a target.
- `ToneSource`: bursts of a sine, a local source the player mixes in.

`sdkconfig.defaults` raises lwIP's UDP receive queue to 32 datagrams. A firmware that uses the
component lists it in its `SDKCONFIG_DEFAULTS`.

## `StreamBufferConfig`

| Field | Default | What it is |
|---|---|---|
| `sample_rate_hz` | 48000 | The streams' sample rate. |
| `depth_ms` | 1000 | The ring's length: the most audio the buffer holds ahead of the read position. A counter that jumps by more than this, either way, starts a new stream. |
| `lead_ms` | 200 | How far ahead of the read position a stream's first packet lands. At most the depth. |
| `grace_ms` | 500 | How long a stream that ran dry waits for its next packet. The next packet within it resumes the stream and counts an underrun. After it, the stream has ended. |

A stream that keeps ahead of real time sits on top of the lead, so the depth must hold the lead,
the sender's lead over real time, and the jitter. IntercomServer keeps 500 ms ahead: the defaults
then buffer 700 ms and leave 300 ms spare. At 48 kHz the ring takes 96 KB a second.

`StreamBuffer::ring_samples(config)` gives the ring's length in samples. The caller allocates the
ring, so it picks PSRAM or internal RAM, and passes it to the constructor. `StreamBuffer` is not
thread-safe: the caller holds a lock around it.

## `Gain`

`Gain(volume, min_db, max_db)` maps a volume above 0 linearly onto `min_db` to `max_db`, by default
-40 dB to 0 dB, so 0.5 is -20 dB. A change of the volume ramps across the next block.

## Tests

`test/` is a GoogleTest build of the pure core. Its configure fails when the core includes anything
but the C++ standard library. esp-libs has no gates of its own, so ToiletAutomation's
`kc project test` runs it:

    cmake -S test -B build-test -G Ninja && cmake --build build-test && ctest --test-dir build-test --output-on-failure
