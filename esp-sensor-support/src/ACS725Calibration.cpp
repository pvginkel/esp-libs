#include "support.h"

#include "ACS725Calibration.h"

#include <cmath>

LOG_TAG(ACS725Calibration);

void ACS725Calibration::reset() {
    _window.reset();
    _total_samples = 0;
    _consecutive_stable = 0;
    _prev_window_mean = 0;
    _have_prev_window = false;
    _in_progress = true;
    _result = {};
}

bool ACS725Calibration::add_sample(float mv) {
    if (!_in_progress) {
        return true;  // Already finished
    }

    _total_samples++;
    _window.add(mv);

    // Check for timeout
    if (_total_samples >= _config.timeout_samples) {
        finish(false, _window.mean, _window.std_dev());
        return true;
    }

    // Process completed windows
    if (_window.n >= _config.window_samples) {
        float std_dev = _window.std_dev();
        float window_mean = _window.mean;

        bool is_stable = std_dev <= _config.max_std_dev_mv;

        // Check for drift if we have a previous window
        if (is_stable && _have_prev_window) {
            float drift = std::fabs(window_mean - _prev_window_mean);
            if (drift > _config.max_drift_mv) {
                is_stable = false;
            }
        }

        if (is_stable) {
            _consecutive_stable++;
            if (_consecutive_stable >= _config.stable_windows) {
                finish(true, window_mean, std_dev);
                return true;
            }
        } else {
            _consecutive_stable = 0;
        }

        _prev_window_mean = window_mean;
        _have_prev_window = true;
        _window.reset();
    }

    return false;
}

void ACS725Calibration::finish(bool success, float zero_mv, float std_dev) {
    _in_progress = false;
    _result.success = success;
    _result.zero_mv = zero_mv;
    _result.final_std_dev = std_dev;
    _result.total_samples = _total_samples;
}
