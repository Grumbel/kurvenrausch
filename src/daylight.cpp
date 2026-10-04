// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "daylight.hpp"

#include <algorithm>
#include <cmath>

namespace racer {

namespace {

constexpr float pi = 3.14159265f;

float smoothstep(float a, float b, float x) {
    const float t = std::clamp((x - a) / (b - a), 0.f, 1.f);
    return t * t * (3.f - 2.f * t);
}

// The lamps of the vehicle sprites (sprites.cpp) and the stars
// (background.cpp), lit or dim: at night they shine.
constexpr uint32_t emissive[] = {
    0xffff543c, 0xfffff0e0, 0xff8c1212, 0xffc84438, 0xffff3020, // tail and brake lights
    0xffffc038, 0xfffff4c0,                                     // indicators, lit
    0xfff0ecc8, 0xffffffff,                                     // headlights (front views)
    0xffff3030, 0xff4070ff, 0xfffff8f0,                         // the police lightbar
    0xffe8eeff, 0xffb8c8ff,                                     // stars
};

} // namespace

Daylight daylight_at(float hour) {
    // The sun's height: 1 at noon, 0 at six in the morning and evening.
    const float e = std::sin((hour - 6.f) / 12.f * pi);
    Daylight d;
    d.level = night_level + (1.f - night_level) * smoothstep(-0.15f, 0.2f, e);
    d.glow = std::max(0.f, 1.f - std::abs(e - 0.02f) / 0.22f);
    d.stars = smoothstep(0.f, -0.25f, e);
    d.sun = smoothstep(-0.06f, 0.06f, e);
    return d;
}

float advance_hour(float hour, float seconds) {
    const float h = std::fmod(hour + seconds / day_seconds * 24.f, 24.f);
    return h < 0.f ? h + 24.f : h;
}

RoadTheme at_daytime(const RoadTheme& look, const Daylight& light) {
    RoadTheme r = look;
    r.sky_horizon = blend(look.sky_horizon, Color{0xff, 0x90, 0x50}, 0.7f * light.glow);
    r.sky_top = blend(look.sky_top, Color{0x60, 0x40, 0x90}, 0.4f * light.glow);
    r.sun = blend(look.sun, Color{0xff, 0x70, 0x30}, light.glow);
    // At dawn and dusk the sun shows even where the zone has none at noon.
    r.sun_amount = std::max(look.sun_amount, 0.6f * light.glow) * light.sun;
    r.cloud_tint = blend(look.cloud_tint, Color{0xc0, 0x50, 0x40}, light.glow);
    r.cloud_tint_amount = std::clamp(look.cloud_tint_amount + 0.5f * light.glow, 0.f, 1.f);
    r.stars = light.stars;
    return r;
}

bool night_emissive(Color c) {
    const uint32_t argb = c.argb();
    return std::find(std::begin(emissive), std::end(emissive), argb) != std::end(emissive);
}

void apply_daylight(Framebuffer& fb, const Daylight& light) {
    if (light.level >= 0.999f && light.glow <= 0.001f) return;
    // Per channel: blue at night, warm in the glow of dawn and dusk.
    const float night = (1.f - light.level) / (1.f - night_level);
    const float r = light.level * (1.f - 0.15f * night) * (1.f - 0.05f * light.glow);
    const float g = light.level * (1.f - 0.05f * night) * (1.f - 0.15f * light.glow);
    const float b = light.level * (1.f + 0.45f * night) * (1.f - 0.30f * light.glow);
    uint32_t* px = fb.pixels_mut();
    const int n = fb.width() * fb.height();
    for (int i = 0; i < n; ++i) {
        const uint32_t p = px[i];
        if (night_emissive(Color{static_cast<uint8_t>(p >> 16), static_cast<uint8_t>(p >> 8), static_cast<uint8_t>(p)})) {
            continue;
        }
        const auto ch = [](uint32_t v, float k) { return static_cast<uint32_t>(std::min(255.f, static_cast<float>(v) * k)); };
        px[i] = 0xff000000u | ch((p >> 16) & 0xff, r) << 16 | ch((p >> 8) & 0xff, g) << 8 | ch(p & 0xff, b);
    }
}

void headlight_beam(Framebuffer& fb, const std::vector<uint32_t>& day, const Daylight& light, int horizon,
                    int bottom) {
    const float dark = 1.f - light.level;
    if (dark <= 0.01f) return;
    uint32_t* px = fb.pixels_mut();
    const int w = fb.width(), h = fb.height();
    const float half = static_cast<float>(w) / 2.f;
    for (int y = std::max(horizon + 1, 0); y < std::min(bottom, h); ++y) {
        // Rows nearer the horizon are further away: the wedge narrows to the
        // vanishing point, brightest at mid distance, fading into the dark.
        const float s = static_cast<float>(y - horizon) / static_cast<float>(h - horizon);
        const float reach = smoothstep(0.04f, 0.3f, s) * (1.f - 0.35f * s);
        const float width = s * static_cast<float>(h) * (4.f / 3.f) * 0.55f; // as wide on a wide screen
        for (int x = 0; x < w; ++x) {
            const float across = std::abs(static_cast<float>(x) + 0.5f - half) / std::max(width, 1.f);
            if (across >= 1.f) continue;
            const float k = 0.85f * dark * reach * (1.f - across * across);
            const size_t i = static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x);
            const uint32_t a = px[i], o = day[i];
            const auto mix = [k](uint32_t from, uint32_t to) {
                return static_cast<uint32_t>(static_cast<float>(from) + (static_cast<float>(to) - static_cast<float>(from)) * k);
            };
            px[i] = 0xff000000u | mix((a >> 16) & 0xff, (o >> 16) & 0xff) << 16 | mix((a >> 8) & 0xff, (o >> 8) & 0xff) << 8 |
                    mix(a & 0xff, o & 0xff);
        }
    }
}

} // namespace racer
