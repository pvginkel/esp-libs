#pragma once

#include "TemperatureLight.h"

template <class InterpolateAlgorithm>
class DualTemperatureLight {
    Callback<float> _levelChanged;
    Callback<uint16_t> _temperatureChanged;
    TemperatureLight<InterpolateAlgorithm>* _low;
    TemperatureLight<InterpolateAlgorithm>* _high;
    float _level;
    float _breakStart;
    float _breakEnd;

public:
    DualTemperatureLight(TemperatureLight<InterpolateAlgorithm>& low, TemperatureLight<InterpolateAlgorithm>& high)
        : _low(&low), _high(&high), _level(0), _breakStart(0), _breakEnd(0) {}

    void reconfigure(float breakStart, float breakEnd, uint32_t time = 0) {
        auto level = getLevel();

        _breakStart = breakStart;
        _breakEnd = breakEnd;

        setLevel(level, time);
    }

    void begin() {
        _low->begin();
        _high->begin();
    }

    bool isOn() const { return _level > 0; }
    float getLevel() const { return _level; }
    uint16_t getTemperature() const { return _low->getTemperature(); }
    void onLevelChanged(std::function<void(float)> func) { _levelChanged.add(func); }
    void onTemperatureChanged(std::function<void(uint16_t)> func) { _temperatureChanged.add(func); }
    void onHighWarmDutyCycleChanged(std::function<void(float)> func) { _high->onWarmDutyCycleChanged(func); }
    void onHighColdDutyCycleChanged(std::function<void(float)> func) { _high->onColdDutyCycleChanged(func); }
    void onLowWarmDutyCycleChanged(std::function<void(float)> func) { _low->onWarmDutyCycleChanged(func); }
    void onLowColdDutyCycleChanged(std::function<void(float)> func) { _low->onColdDutyCycleChanged(func); }

    void update() {
        _low->update();
        _high->update();
    }

    void setLevel(float level, uint32_t time = 0) {
        _level = level;

        // Set the high lamp. The high lamp goes from breakStart till 255,
        // regardless of where the low lamp is at.

        if (level < _breakStart) {
            _high->setLevel(0, time);
        } else {
            const auto range = 1.0f - _breakStart;
            const auto offset = (level - _breakStart) / range;
            _high->setLevel(offset, time);
        }

        // Set the low lamp. Until breakStart, the low lamp increases relative
        // to the break start. Between breakStart and breakEnd, the low lamp
        // decreases as the offset between the two.

        if (level < _breakStart) {
            const auto offset = level / _breakStart;
            _low->setLevel(offset, time);
        } else if (level > _breakEnd) {
            _low->setLevel(0, time);
        } else {
            const auto range = _breakEnd - _breakStart;
            const auto offset = (level - _breakStart) / range;
            _low->setLevel(1.0f - offset, time);
        }

        _levelChanged.call(_level);
    }

    void setTemperature(uint16_t temperature, uint32_t time = 0) {
        _low->setTemperature(temperature, time);
        _high->setTemperature(temperature, time);

        _temperatureChanged.call(temperature);
    }

    void resetTransition() {
        _low->resetTransition();
        _high->resetTransition();
    }
};

typedef DualTemperatureLight<CIE1931InterpolateAlgorithm> NaturalDualTemperatureLight;
