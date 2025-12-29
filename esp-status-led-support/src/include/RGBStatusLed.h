#pragma once

#include "StatusLed.h"

struct RGB {
    float r;
    float g;
    float b;
};

namespace Colors {
constexpr RGB Black = {0.0f, 0.0f, 0.0f};
constexpr RGB White = {1.0f, 1.0f, 1.0f};
constexpr RGB Red = {1.0f, 0.0f, 0.0f};
constexpr RGB Green = {0.0f, 1.0f, 0.0f};
constexpr RGB Blue = {0.0f, 0.0f, 1.0f};
constexpr RGB Yellow = {1.0f, 1.0f, 0.0f};
constexpr RGB Orange = {1.0f, 0.5f, 0.0f};
constexpr RGB Purple = {0.5f, 0.0f, 1.0f};
constexpr RGB Magenta = {1.0f, 0.0f, 1.0f};
constexpr RGB Cyan = {0.0f, 1.0f, 1.0f};
constexpr RGB Pink = {1.0f, 0.4f, 0.7f};
constexpr RGB Lime = {0.5f, 1.0f, 0.0f};
constexpr RGB Teal = {0.0f, 0.5f, 0.5f};
constexpr RGB Indigo = {0.3f, 0.0f, 0.5f};
constexpr RGB Gold = {1.0f, 0.84f, 0.0f};
constexpr RGB Coral = {1.0f, 0.5f, 0.31f};
}  // namespace Colors

class RGBStatusLed : public virtual StatusLed {
    RGB _color{};

public:
    void set_color(RGB color) {
        _color = color;

        config_changed();
    }

    RGB get_color() const { return _color; }
};
