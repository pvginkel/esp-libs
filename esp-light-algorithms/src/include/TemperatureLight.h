#pragma once

#include "Light.h"

class TemperatureLightBase {
    Callback<float> _levelChanged;
    Callback<uint16_t> _temperatureChanged;
    LinearLight _cold;
    LinearLight _warm;
    uint16_t _minimumTemperature;
    uint16_t _maximumTemperature;
    float _minimumLevel;
    float _maximumLevel;
    float _level;
    uint16_t _temperature;

public:
    TemperatureLightBase(uint16_t minimumTemperature, uint16_t maximumTemperature)
        : _minimumTemperature(minimumTemperature),
          _maximumTemperature(maximumTemperature),
          _minimumLevel(0.0f),
          _maximumLevel(1.0f),
          _level(0.0f),
          _temperature(minimumTemperature) {}

    void reconfigure(float minimumLevel, float maximumLevel, uint32_t time = 0) {
        _minimumLevel = minimumLevel;
        _maximumLevel = maximumLevel;

        updateLevelAndTemperature(time);
    }

    void begin() {
        _cold.begin();
        _warm.begin();
    }

    bool isOn() const { return _level > 0; }
    float getLevel() const { return _level; }
    uint16_t getTemperature() const { return _temperature; }
    void onLevelChanged(std::function<void(float)> func) { _levelChanged.add(func); }
    void onTemperatureChanged(std::function<void(uint16_t)> func) { _temperatureChanged.add(func); }
    void onColdDutyCycleChanged(std::function<void(float)> func) { _cold.onDutyCycleChanged(func); }
    void onWarmDutyCycleChanged(std::function<void(float)> func) { _warm.onDutyCycleChanged(func); }

    void update() {
        _cold.update();
        _warm.update();
    }

    void setLevel(float level, uint32_t time = 0) {
        _level = level;

        updateLevelAndTemperature(time);

        _levelChanged.call(_level);
    }

    void setTemperature(uint16_t temperature, uint32_t time = 0);

    void resetTransition() {
        _cold.resetTransition();
        _warm.resetTransition();
    }

protected:
    virtual float interpolate(float level) = 0;

private:
    void updateLevelAndTemperature(uint32_t time = 0);
};

template <class InterpolateAlgorithm>
class TemperatureLight : public TemperatureLightBase {
public:
    TemperatureLight(uint16_t minimumTemperature, uint16_t maximumTemperature)
        : TemperatureLightBase(minimumTemperature, maximumTemperature) {}

protected:
    float interpolate(float level) override { return InterpolateAlgorithm::interpolate(level); }
};

typedef TemperatureLight<CIE1931InterpolateAlgorithm> NaturalTemperatureLight;
