#pragma once

#include <cstdint>
#include <memory>

// Sensirion's gas index algorithm for VOC (BSD-3-Clause, vendored in
// src/sensirion_gas_index_algorithm/), sampled once a second. It turns the SGP40's raw signal into
// the VOC index, 1 to 500, where 100 is the learned average of the past hours.
//
// The algorithm starts with a blackout of 45 s, in which it learns and gives no index, and then
// learns for hours more. It runs with whatever compensation the raw signal was measured at.
class VOCIndex {
    struct Params;

    std::unique_ptr<Params> _params;
    bool _blackout{true};

public:
    VOCIndex();
    ~VOCIndex();

    VOCIndex(const VOCIndex&) = delete;
    VOCIndex& operator=(const VOCIndex&) = delete;

    // Feeds one raw signal, taken a second after the one before, and returns the VOC index. In the
    // blackout it returns 0.
    int32_t process(uint16_t raw);

    // True while the algorithm is in its blackout: before the first process(), and while process()
    // returns no index.
    bool is_blackout() const { return _blackout; }

    // Starts over, with the blackout, as after a gap in the samples.
    void reset();
};
