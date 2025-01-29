#include "support.h"

#include "StatusControl.h"

#include "esp_timer.h"

LOG_TAG(StatusControl);

static uint32_t millis() { return esp_timer_get_time() / 1000; }

void StatusControl::setBounce(Bounce bounce) { _bounce = bounce; }

void StatusControl::setConnected(connection_status_t connected) {
    if (_connected != connected) {
        _lastStatusChange = millis();
        _connected = connected;
    }
}

void StatusControl::reportRemaining(int remaining) {
    if (_remainingReported != remaining) {
        _remainingReported = remaining;
        _resetCountdown.call(remaining);

        if (remaining > 0) {
            ESP_LOGI(TAG, "Resetting in %d", remaining);
        } else if (remaining == 0) {
            ESP_LOGI(TAG, "Resetting NOW");
        }
    }
}

void StatusControl::update() {
    _bounce.update();

    auto currentMillis = millis();

    if (_bounce.read()) {
        if (!_isHigh) {
            _lastStatusChange = currentMillis;
            _isHigh = true;
            _click.call();
            setLevel(1);
        }
        if (_bounce.currentDuration() > _initialDelay) {
            auto duration = _bounce.currentDuration() - _initialDelay;
            auto steps = duration / _stepTime;
            int remaining = _steps - (int)steps;

            if (remaining <= 0) {
                if (!_resetRaised) {
                    _resetRaised = true;

                    setLevel(0);

                    reportRemaining(remaining);

                    _reset.call();
                }
            } else {
                reportRemaining(remaining);
            }
        }
    } else if (_isHigh) {
        _isHigh = false;
        _resetRaised = false;
        _lastStatusChange = currentMillis;
        reportRemaining(-1);
        setLevel(0);
    }

    // Blink progressively shorter while the button is down and the
    // reset hasn't been raised.

    if (_isHigh && !_resetRaised) {
        // The algorithm is to wait for 20% of the time until the
        // the reset will be raised to toggle the led.

        auto total = _initialDelay + _steps * _stepTime;
        auto target = currentMillis - _bounce.currentDuration() + total;
        auto remaining = target - _lastStatusChange;
        auto nextToggle = (remaining * 10) / 100;
        if (nextToggle < 25) {
            nextToggle = 25;
        }
        auto progress = currentMillis - _lastStatusChange;

        if (progress >= nextToggle) {
            _lastStatusChange = currentMillis;
            setLevel(_lastLevel ? 0 : 1);
        }
    } else if (_connected == CONNECTION_STATUS_CONNECTING) {
        // Fade the led in and out while we're connecting.

        auto duration = currentMillis - _lastStatusChange;
        auto progress = duration % _fadePeriod;
        bool inverse = (duration / _fadePeriod) % 2 == 0;
        auto level = (float)progress / _fadePeriod;

        if (inverse) {
            level = 1.0f - level;
        }
        setLevel(level);
    } else if (_levelSet) {
        _levelSet = false;
        _ledBrightnessReset.call();
    }
}

void StatusControl::setLevel(float level) {
    if (level != _lastLevel || !_levelSet) {
        _lastLevel = level;
        _levelSet = true;
        _ledBrightnessChanged.call(level);
    }
}
