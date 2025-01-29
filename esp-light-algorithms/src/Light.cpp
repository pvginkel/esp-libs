#include "support.h"

#include "Light.h"

LOG_TAG(Light);

RGB hsi2rgb(float H, float S, float I) {
    float r, g, b;
    H = fmodf(H, 360);                // cycle H around to 0-360 degrees
    H = 3.14159f * H / 180.0f;        // Convert to radians.
    S = S > 0 ? (S < 1 ? S : 1) : 0;  // clamp S and I to interval [0,1]
    I = I > 0 ? (I < 1 ? I : 1) : 0;

    // Math! Thanks in part to Kyle Miller.
    if (H < 2.09439f) {
        r = I / 3 * (1 + S * cosf(H) / cosf(1.047196667f - H));
        g = I / 3 * (1 + S * (1 - cosf(H) / cosf(1.047196667f - H)));
        b = I / 3 * (1 - S);
    } else if (H < 4.188787f) {
        H = H - 2.09439f;
        g = I / 3 * (1 + S * cosf(H) / cos(1.047196667f - H));
        b = I / 3 * (1 + S * (1 - cosf(H) / cosf(1.047196667f - H)));
        r = I / 3 * (1 - S);
    } else {
        H = H - 4.188787f;
        b = I / 3 * (1 + S * cosf(H) / cosf(1.047196667f - H));
        r = I / 3 * (1 + S * (1 - cosf(H) / cosf(1.047196667f - H)));
        g = I / 3 * (1 - S);
    }

    return {r, g, b};
}

uint16_t mired2kelvin(uint16_t mired) { return 1000000 / mired; }

uint16_t kelvin2mired(uint16_t kelvin) { return 1000000 / kelvin; }

float scaleLightLevel(float level, float minimumLevel, float maximumLevel) {
    if (level > 0 && (minimumLevel != 0.0f || maximumLevel != 1.0f)) {
        return minimumLevel + level * (maximumLevel - minimumLevel);
    }

    return level;
}

void LightBase::reconfigure(float minimumLevel, float maximumLevel, uint32_t time) {
    auto level = getLevel();

    _minimumLevel = minimumLevel;
    _maximumLevel = maximumLevel;

    setLevel(level, time);
}

void LightBase::begin() { resetTransition(); }

void LightBase::update() {
    if (!_transitionStart) {
        return;
    }

    auto currentMillis = esp_timer_get_time() / 1000;
    auto diff = currentMillis - _transitionStart;

    if (diff >= _transitionTime) {
        resetTransition();
    } else if (currentMillis > _lastUpdate) {
        // Interpolate the level.

        float progress = float(diff) / float(_transitionTime);

        float level = _startLevel + (getScaledLevel() - _startLevel) * progress;

        _actualLevel = level;
        _lastUpdate = currentMillis;

        updateDutyCycle();
    }
}

void LightBase::setLevel(float level, uint32_t time) {
    if (level < 0.0f) {
        level = 0.0f;
    }
    if (level > 1.0f) {
        level = 1.0f;
    }

    _level = level;

    if (time == 0) {
        resetTransition();
    } else {
        _startLevel = _actualLevel;
        _transitionStart = esp_timer_get_time() / 1000;
        _transitionTime = time;

        // Let update handle setting the value.
    }

    _levelChanged.call(_level);
}

void LightBase::resetTransition() {
    auto scaledLevel = getScaledLevel();

    _startLevel = scaledLevel;
    _transitionStart = 0;
    _transitionTime = 0;

    _actualLevel = scaledLevel;
    _lastUpdate = esp_timer_get_time() / 1000;

    updateDutyCycle();
}

void LightBase::updateDutyCycle() {
    const auto realValue = interpolate(_actualLevel);

    ESP_LOGD(TAG, "Setting light pin to %f duty cycle %f", _actualLevel, realValue);

    _dutyCycleChanged.call(realValue);
}

float CIE1931InterpolateAlgorithm::interpolate(float level) {
    /*
       For CIE, the following formulas have  Y as luminance, and
       Yn is the luminance of a white reference (basically, max luminance).
       This assumes a perceived lightness value L* between 0 and 100,
       and  a luminance value Y of 0-1.0.
       if L* > 8:  Y = ((L* + 16) / 116)^3 * Yn
       if L* <= 8: Y = L* *903.3 * Yn
     */

    float l = level * 100.0f;
    float Y;
    if (l <= 8.0f) {
        Y = (l / 903.3f);
    } else {
        float foo = (l + 16.0f) / 116.0f;
        Y = powf(foo, 3);
    }

    return Y;
}