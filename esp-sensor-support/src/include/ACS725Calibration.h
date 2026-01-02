#pragma once

#include <cmath>
#include <cstdint>

// Stability-based zero calibration for ACS725 current sensor.
// Waits for readings to stabilize (low std dev, no drift) before accepting calibration.
class ACS725Calibration {
public:
    struct Config {
        float max_std_dev_mv = 10.0f;      // Max std dev to consider "stable"
        float max_drift_mv = 1.0f;         // Max mean drift during stable period
        uint32_t window_samples = 1000;    // Samples per stability window (1s at 1kHz)
        uint32_t stable_windows = 3;       // Consecutive stable windows required
        uint32_t timeout_samples = 60000;  // Timeout (60s at 1kHz)
    };

    struct Result {
        bool success = false;
        float zero_mv = 0;
        float final_std_dev = 0;
        uint32_t total_samples = 0;
    };

    ACS725Calibration(const Config& config) : _config(config) {}

    // Reset calibration state. Call before starting a new calibration.
    void reset();

    // Add a sample (in mV). Returns true when calibration is complete.
    // Call result() after this returns true.
    bool add_sample(float mv);

    // Get the calibration result. Only valid after add_sample() returns true.
    const Result& result() const { return _result; }

    // Check if calibration is still in progress
    bool in_progress() const { return _in_progress; }

private:
    // Welford's online algorithm state for current window
    struct WindowStats {
        size_t n = 0;
        float mean = 0;
        float m2 = 0;

        void reset() {
            n = 0;
            mean = 0;
            m2 = 0;
        }

        void add(float x) {
            n++;
            float delta = x - mean;
            mean += delta / (float)n;
            float delta2 = x - mean;
            m2 += delta * delta2;
        }

        float variance() const { return n > 1 ? m2 / (float)(n - 1) : 0; }
        float std_dev() const { return std::sqrt(variance()); }
    };

    Config _config;
    Result _result;
    bool _in_progress = false;

    WindowStats _window;
    uint32_t _total_samples = 0;
    uint32_t _consecutive_stable = 0;
    float _prev_window_mean = 0;
    bool _have_prev_window = false;

    void finish(bool success, float zero_mv, float std_dev);
};
