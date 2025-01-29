#pragma once

#include <functional>

#include "Bounce2.h"
#include "Callback.h"

enum connection_status_t {
    CONNECTION_STATUS_NOT_CONNECTED,
    CONNECTION_STATUS_CONNECTING,
    CONNECTION_STATUS_CONNECTED,
};

class StatusControl {
    Bounce _bounce;
    bool _isHigh = false;
    Callback<void> _click;
    Callback<void> _reset;
    Callback<int> _resetCountdown;
    Callback<float> _ledBrightnessChanged;
    Callback<void> _ledBrightnessReset;
    uint32_t _lastStatusChange{};
    connection_status_t _connected = CONNECTION_STATUS_NOT_CONNECTED;
    uint32_t _initialDelay;
    int _steps;
    uint32_t _stepTime;
    bool _resetRaised{};
    uint32_t _fadePeriod;
    int _remainingReported = -1;
    float _lastLevel;
    bool _levelSet{};

public:
    StatusControl(uint32_t initialDelay = 1000, int steps = 4, uint32_t stepTime = 1000, uint32_t fadePeriod = 1000)
        : _initialDelay(initialDelay), _steps(steps), _stepTime(stepTime), _fadePeriod(fadePeriod) {}

    void setBounce(Bounce bounce);
    void setConnected(connection_status_t connected);
    void reportRemaining(int remaining);
    void update();
    void onClick(std::function<void(void)> func) { _click.add(func); }
    void onReset(std::function<void(void)> func) { _reset.add(func); }
    void onResetCountdown(std::function<void(int)> func) { _resetCountdown.add(func); }
    void onLedBrightnessChanged(std::function<void(float)> func) { _ledBrightnessChanged.add(func); }
    void onLedBrightnessReset(std::function<void(void)> func) { _ledBrightnessReset.add(func); }

private:
    void setLevel(float level);
};
