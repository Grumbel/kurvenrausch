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
    0xffffd888,                                                 // lit windows in town
    0xffffecb0, 0xfffffff0,                                     // street lamps
    0xffff40c0, 0xff40f0ff, 0xffffe060, 0xfff0f4ff,             // neon on the Strip, the pyramid's beam
    0xff80c0ff,                                                 // the time car's coils
    0xfffff0a0,                                                 // Golden Gate crown lights
    0xffe0e4ec, 0xffb0b8c8, 0xffc8d0e0,                         // the moon
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

SkyBody sun_position(float hour) {
    // Simple equatorial day: zenith at noon, on the horizon at 6 and 18,
    // below at midnight. Azimuth runs left (east) at sunrise to right (west)
    // at sunset.
    const float a = (hour - 12.f) / 12.f * PI;
    return SkyBody{std::cos(a), std::sin(a)};
}

SkyBody moon_position(float hour) {
    // Roughly opposite the sun: high at midnight, low at noon.
    return sun_position(std::fmod(hour + 12.f, 24.f));
}

Daylight lit_by(Daylight light, float glow) {
    light.level += (1.f - light.level) * 0.3f * std::clamp(glow, 0.f, 1.f);
    return light;
}

RoadTheme at_daytime(const RoadTheme& look, const Daylight& light) {
    RoadTheme r = look;
    // Over a city the night sky glows orange from its lights.
    const float dark = std::clamp((1.f - light.level) / (1.f - night_level), 0.f, 1.f);
    r.sky_horizon = blend(look.sky_horizon, Color{0xe0, 0x90, 0x50}, 0.45f * dark * look.night_glow);
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

void headlight_beam(Framebuffer& fb, const std::vector<uint32_t>& day, const Daylight& light,
                    const std::vector<float>& row_depth, const Beam& beam) {
    const float dark = 1.f - light.level;
    if (dark <= 0.01f) return;
    uint32_t* px = fb.pixels_mut();
    const int w = fb.width(), h = std::min(fb.height(), static_cast<int>(row_depth.size()));
    for (int y = 0; y < std::min(beam.bottom, h); ++y) {
        const float depth = row_depth[static_cast<size_t>(y)];
        const float ahead = depth - beam.start; // from the lamps
        if (depth <= 0.f || ahead <= 0.f || ahead > 4.f * beam_reach) continue;
        // Bright from just past the lamps, fading with distance.
        const float reach = smoothstep(0.f, 200.f, ahead) / (1.f + (ahead / beam_reach) * (ahead / beam_reach));
        const float px_per_unit = beam.camera_depth / depth * beam.x_scale;
        const float half = (beam_half_width + beam_spread * ahead) * px_per_unit;
        const float mid = beam.center + beam.aim * ahead * px_per_unit;
        const int x0 = std::max(0, static_cast<int>(mid - half)), x1 = std::min(w, static_cast<int>(mid + half) + 1);
        for (int x = x0; x < x1; ++x) {
            const float across = std::abs(static_cast<float>(x) + 0.5f - mid) / half;
            if (across >= 1.f) continue;
            // Even across most of its width, a brighter spot in the middle,
            // soft at the sides.
            const float edge = smoothstep(1.f, 0.55f, across) * (0.75f + 0.25f * (1.f - across * across));
            const float k = 0.9f * dark * reach * edge;
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

void street_lights(Framebuffer& fb, const std::vector<uint32_t>& day, const std::vector<uint32_t>& ground,
                   const Daylight& light, const std::vector<float>& row_depth, const std::vector<LampSpot>& lamps,
                   float camera_depth, float x_scale) {
    const float dark = 1.f - light.level;
    if (dark <= 0.01f || lamps.empty() || ground.size() != day.size()) return;
    uint32_t* px = fb.pixels_mut();
    const int w = fb.width(), h = std::min(fb.height(), static_cast<int>(row_depth.size()));
    for (int y = 0; y < h; ++y) {
        const float depth = row_depth[static_cast<size_t>(y)];
        if (depth <= 0.f) continue;
        const float px_per_unit = camera_depth / depth * x_scale;
        for (const LampSpot& lamp : lamps) {
            const float dz = depth - lamp.depth;
            if (std::abs(dz) >= lamp.reach) continue;
            // Sodium yellow, headlight white, tail light red; the red only a glow.
            const float tint_g = lamp.glow == Glow::Street ? 0.85f : lamp.glow == Glow::Tail ? 0.25f : 1.f;
            const float tint_b = lamp.glow == Glow::Street ? 0.55f : lamp.glow == Glow::Tail ? 0.2f : 0.95f;
            const float strength = lamp.glow == Glow::Tail ? 0.5f : lamp.glow == Glow::Head ? 0.8f : 0.9f;
            const float half = std::sqrt(lamp.reach * lamp.reach - dz * dz);
            const int x0 = std::max(0, static_cast<int>(lamp.x - half * px_per_unit));
            const int x1 = std::min(w, static_cast<int>(lamp.x + half * px_per_unit) + 1);
            for (int x = x0; x < x1; ++x) {
                const float dx = (static_cast<float>(x) + 0.5f - lamp.x) / px_per_unit;
                const float r2 = (dx * dx + dz * dz) / (lamp.reach * lamp.reach);
                if (r2 >= 1.f) continue;
                const float k = strength * dark * (1.f - r2) * (1.f - r2);
                const size_t i = static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x);
                if (day[i] != ground[i]) continue; // something stands there
                const uint32_t a = px[i], o = day[i];
                const auto mix = [k](uint32_t from, uint32_t to, float tint) {
                    const float t = static_cast<float>(to) * tint;
                    return static_cast<uint32_t>(std::min(255.f, static_cast<float>(from) + (t - static_cast<float>(from)) * k));
                };
                px[i] = 0xff000000u | mix((a >> 16) & 0xff, (o >> 16) & 0xff, 1.f) << 16 |
                        mix((a >> 8) & 0xff, (o >> 8) & 0xff, tint_g) << 8 | mix(a & 0xff, o & 0xff, tint_b);
            }
        }
    }
}

} // namespace racer
