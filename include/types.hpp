#pragma once
#include <cstdint>
#include <cmath>
#include <algorithm>

namespace racer {

using Entity = uint32_t;
constexpr Entity INVALID_ENTITY = UINT32_MAX;

struct Color {
    uint8_t r = 0, g = 0, b = 0, a = 255;

    constexpr Color() = default;
    constexpr Color(uint8_t r_, uint8_t g_, uint8_t b_, uint8_t a_ = 255)
        : r(r_), g(g_), b(b_), a(a_) {}

    // Pack for SDL_PIXELFORMAT_ARGB8888 (common on little-endian Linux)
    // Memory layout: A R G B in the 32-bit word as 0xAARRGGBB
    constexpr uint32_t to_u32() const {
        return (static_cast<uint32_t>(a) << 24) |
               (static_cast<uint32_t>(r) << 16) |
               (static_cast<uint32_t>(g) << 8)  |
               (static_cast<uint32_t>(b));
    }
};

namespace Palette {
    constexpr Color SkyTop{0x5B, 0x9B, 0xD5};
    constexpr Color SkyBottom{0xC8, 0xE0, 0xF0};
    constexpr Color Horizon{0xE8, 0xD0, 0xA0};
    constexpr Color GrassLight{0x4A, 0xA0, 0x3A};
    constexpr Color GrassDark{0x3A, 0x80, 0x2A};
    constexpr Color RoadLight{0x6A, 0x6A, 0x6A};
    constexpr Color RoadDark{0x58, 0x58, 0x58};
    constexpr Color RumbleLight{0xE8, 0xE8, 0xE8};
    constexpr Color RumbleDark{0xC8, 0x28, 0x28};
    constexpr Color Lane{0xF0, 0xF0, 0xF0};
    constexpr Color Cliff{0x78, 0x58, 0x38};
    constexpr Color TreeGreen{0x28, 0x88, 0x28};
    constexpr Color TreeDark{0x18, 0x68, 0x18};
    constexpr Color TreeTrunk{0x68, 0x48, 0x28};
    constexpr Color CarBody{0xD0, 0x20, 0x20};
    constexpr Color CarDark{0x90, 0x10, 0x10};
    constexpr Color CarGlass{0x40, 0x60, 0x90};
    constexpr Color White{0xFF, 0xFF, 0xFF};
    constexpr Color Black{0x00, 0x00, 0x00};
    constexpr Color Fog{0xC8, 0xD8, 0xE8};
}

inline float clamp(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}

inline float lerp(float a, float b, float t) {
    return a + (b - a) * t;
}

} // namespace racer
