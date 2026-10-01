#pragma once
#include <cstdint>
#include <cmath>
#include <vector>
#include <string>
#include <array>

namespace racer {

using Entity = uint32_t;
constexpr Entity INVALID_ENTITY = UINT32_MAX;
constexpr float PI = 3.14159265358979323846f;

struct Vec2 {
    float x = 0.f, y = 0.f;
    Vec2() = default;
    constexpr Vec2(float x_, float y_) : x(x_), y(y_) {}
    constexpr Vec2 operator+(const Vec2& o) const { return {x + o.x, y + o.y}; }
    constexpr Vec2 operator-(const Vec2& o) const { return {x - o.x, y - o.y}; }
    constexpr Vec2 operator*(float s) const { return {x * s, y * s}; }
};

struct Color {
    uint8_t r, g, b, a;
    constexpr Color() : r(0), g(0), b(0), a(255) {}
    constexpr Color(uint8_t r_, uint8_t g_, uint8_t b_, uint8_t a_ = 255) : r(r_), g(g_), b(b_), a(a_) {}
    constexpr uint32_t to_rgba() const {
        return (static_cast<uint32_t>(a) << 24) | (static_cast<uint32_t>(b) << 16) |
               (static_cast<uint32_t>(g) << 8) | static_cast<uint32_t>(r);
    }
    static constexpr Color from_hex(uint32_t hex) {
        return Color(static_cast<uint8_t>((hex >> 16) & 0xFF),
                     static_cast<uint8_t>((hex >> 8) & 0xFF),
                     static_cast<uint8_t>(hex & 0xFF));
    }
};

// Classic palette inspired by OutRun / Lotus
namespace Palette {
    constexpr Color SkyTop{0x40, 0x80, 0xC0};
    constexpr Color SkyBottom{0xA0, 0xD0, 0xF0};
    constexpr Color Horizon{0xE0, 0xC0, 0x80};
    constexpr Color GrassLight{0x40, 0x90, 0x30};
    constexpr Color GrassDark{0x30, 0x70, 0x20};
    constexpr Color RoadLight{0x60, 0x60, 0x60};
    constexpr Color RoadDark{0x50, 0x50, 0x50};
    constexpr Color RumbleLight{0xE0, 0xE0, 0xE0};
    constexpr Color RumbleDark{0xC0, 0x20, 0x20};
    constexpr Color Lane{0xE0, 0xE0, 0xE0};
    constexpr Color Cliff{0x70, 0x50, 0x30};
    constexpr Color TreeGreen{0x20, 0x80, 0x20};
    constexpr Color TreeTrunk{0x60, 0x40, 0x20};
    constexpr Color CarRed{0xC0, 0x20, 0x20};
    constexpr Color CarDark{0x80, 0x10, 0x10};
    constexpr Color White{0xFF, 0xFF, 0xFF};
    constexpr Color Black{0x00, 0x00, 0x00};
}

} // namespace racer
