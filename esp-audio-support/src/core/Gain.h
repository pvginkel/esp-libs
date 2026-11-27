#pragma once

#include <cstddef>
#include <cstdint>

// The volume stage: a volume from 0 to 1 as a gain in dB. 0 mutes. Above it, the volume maps
// linearly onto the range in dB, so 1 is `max_db` and a volume just above 0 is `min_db`. Equal
// steps of the volume then sound like equal steps of loudness.
//
// A change of the volume ramps across the next block, so it does not click.
class Gain {
public:
    static constexpr float DEFAULT_MIN_DB = -40.0f;
    static constexpr float DEFAULT_MAX_DB = 0.0f;

private:
    float _min_db;
    float _max_db;
    float _volume{};
    // The linear factor the last block ended on, and the one the volume asks for.
    float _current{};
    float _target{};

public:
    explicit Gain(float volume = 1.0f, float min_db = DEFAULT_MIN_DB, float max_db = DEFAULT_MAX_DB);

    // Clamped to 0 to 1. A value that is not a number mutes.
    void set_volume(float volume);
    float get_volume() const { return _volume; }

    // The gain the volume maps to: negative infinity when muted.
    float get_db() const;

    // Scales the block in place, rounding to the nearest sample and saturating.
    void process(int16_t* samples, size_t count);

    // The volume's gain in dB on a range: negative infinity for 0.
    static float volume_to_db(float volume, float min_db, float max_db);
};
