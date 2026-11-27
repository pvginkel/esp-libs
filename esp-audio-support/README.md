# Audio playback support for the ESP32

A stream of 16-bit mono PCM over UDP, buffered against the network's jitter, mixed from several
sources and played on an I2S amplifier such as the MAX98357.

The pure core is in `src/core/`. It includes the C++ standard library only, and `test/` builds it
natively:

- `StreamBuffer`: the jitter buffer. Each packet is a 4-byte big-endian counter, then 16-bit
  little-endian PCM. It places each packet by its counter and plays a lost one as silence, and it
  mixes the streams of several sources, each keyed by an opaque `SourceId` the caller picks. It
  counts what it does in `stats()`.
- `Gain`: the volume, from 0 to 1, as a gain in dB. 0 mutes.
- `AutoVolume`: an optional stage that holds the level at or below a target.
- `ToneSource`: bursts of a sine, a local source the player mixes in.

The ESP-IDF side is in `src/`, with its headers in `src/include/`:

- `I2SOutput`: the I2S channel, TX only, on its own task.
- `AudioPlayer`: the stream buffer and a tone, through the volume, out on an `I2SOutput`.

Nothing opens the UDP port yet. A receiver hands its packets to `AudioPlayer::push`.

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

## `AudioPlayer`

`AudioPlayer::begin(config)` allocates the ring and starts the output, idle. Each block the output
asks for goes through these steps, in order:

1. the stream buffer's next block;
2. `AutoVolume`, when it is on;
3. the tone, mixed in;
4. the volume, last, so it sets the level of the tone as well as the stream.

The output plays while the stream buffer has a stream, its grace included, or the tone plays. It
stops when neither has anything. Every function may be called from any task: a lock guards the
buffer, the tone and the volume against the output's task.

- `set_volume(volume)`, `get_volume()` and `get_volume_db()`: the volume, from 0 to 1, as `Gain`
  maps it.
- `play_tone(frequency_hz, duration_ms, level, count, pause_ms)`: bursts of a sine, `level` of full
  scale before the volume. It replaces a tone that still plays.
- `push(source, packet, length)`: a stream's packet into the stream buffer.
- `is_playing()` and `get_stats()`: the stream buffer's counters, how far it reaches ahead, and the
  output's counters.

### `AudioPlayerConfig`

| Field | Default | What it is |
|---|---|---|
| `i2s_port` | `I2S_NUM_0` | The I2S port. |
| `bclk_gpio`, `ws_gpio`, `dout_gpio` | none | The amplifier's BCLK, LRC and DIN. |
| `format` | `PHILIPS` | `PHILIPS` or `MSB`. ToiletAutomation's board was brought up on Philips, and Intercom's boards use MSB. |
| `sample_rate_hz` | 48000 | The stream's rate, and the output's. |
| `depth_ms`, `lead_ms`, `grace_ms` | 1000, 200, 500 | The stream buffer's, as `StreamBufferConfig` has them. |
| `buffer_in_psram` | false | Whether the ring goes in PSRAM rather than internal RAM. |
| `udp_port` | 11106 | The port the stream comes in on, Intercom's. Nothing opens it yet. |
| `volume` | 1 | The volume it starts at. |
| `min_db`, `max_db` | -40, 0 | The range in dB that the volume maps onto. |
| `auto_volume`, `auto_volume_target_db` | false, -18 | `AutoVolume` on the stream, and its target. |
| `task_priority`, `task_core` | 7, no affinity | The output's task. |

## `I2SOutput`

`I2SOutput::begin(config, source, tap)` sets the channel up and starts its task, which sleeps until
`start()`. The task then works like this:

1. It takes a block from its source. When the source has nothing, it goes back to sleep, so a start
   that comes while the output plays does nothing.
2. It preloads the DMA with silence, so the amplifier has its clocks before the first sample.
3. It enables the channel and writes the block. The blocking write sets the pace from there.
4. When the source has nothing more, it writes the DMA's depth of silence, so the last block plays
   out, and disables the channel. That stops the clocks, and the MAX98357 mutes.

The stream goes out in the left slot, the one a MAX98357 with its SD pin high plays. The DMA has six
buffers of 240 frames, 30 ms at 48 kHz. A buffer the task did not refill in time plays as silence and
counts as an underrun in `get_stats()`.

**The grace is the stream buffer's alone.** A stream that runs dry plays silence for the grace while
it waits for its next packet (Intercom's finding H3), and the output runs on through it. The output
has no grace of its own: its silence after the last block is the DMA draining, not a wait.

**The played-samples tap** (`I2SOutputTap`) is optional, and both of its functions run on the
output's task:

- `samples(samples, count, time_us)`: each block just before it is written, with the `esp_timer`
  time at which its first sample plays;
- `reset()`: the output starts or has stopped, so the samples after it do not follow on from the
  ones before.

It replaces Intercom's `feed_reference_samples` and `reset_feed_buffer`, for its echo canceller's
reference.

## Tests

`test/` is a GoogleTest build of the pure core. Its configure fails when the core includes anything
but the C++ standard library. esp-libs has no gates of its own, so ToiletAutomation's
`kc project test` runs it:

    cmake -S test -B build-test -G Ninja && cmake --build build-test && ctest --test-dir build-test --output-on-failure
