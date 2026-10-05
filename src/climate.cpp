// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "climate.hpp"

#include <algorithm>
#include <cmath>

namespace racer {

namespace {

float smoothstep(float a, float b, float x) {
    const float t = std::clamp((x - a) / (b - a), 0.f, 1.f);
    return t * t * (3.f - 2.f * t);
}

} // namespace

float WeatherFront::key(int index) const {
    uint32_t h = static_cast<uint32_t>(index) * 0x9e3779b9u ^ seed_;
    h = (h ^ (h >> 16)) * 0x85ebca6bu;
    h = (h ^ (h >> 13)) * 0xc2b2ae35u;
    h ^= h >> 16;
    const float u = static_cast<float>(h >> 8) / 16777216.f;
    return u * std::sqrt(u); // skewed: mostly fair, now and then a storm
}

float WeatherFront::level() const {
    if (forced_ >= 0.f) return std::min(forced_, 1.f);
    const float t = time_ / key_seconds;
    const int i = static_cast<int>(std::floor(t));
    const float f = t - static_cast<float>(i);
    const float ease = f * f * (3.f - 2.f * f);
    return key(i) + (key(i + 1) - key(i)) * ease;
}

RoadTheme weathered(const RoadTheme& look, float level) {
    RoadTheme r = look;
    const float l = std::clamp(level, 0.f, 1.f);
    r.rain = std::clamp(look.rain * (0.3f + 1.2f * l) + look.showers * smoothstep(0.4f, 0.9f, l), 0.f, 1.f);
    r.snowfall = std::clamp(look.snowfall * (0.3f + 1.2f * l), 0.f, 1.f);
    // How much wetter than the zone's usual weather it is (or drier).
    const float extra = (r.rain - look.rain) + (r.snowfall - look.snowfall);
    const float storm = std::clamp(1.6f * extra, 0.f, 1.f);
    const Color grey{0x48, 0x50, 0x5c};
    r.sky_top = blend(look.sky_top, grey, 0.85f * storm);
    r.sky_horizon = blend(look.sky_horizon, Color{0x8c, 0x94, 0x9c}, 0.75f * storm);
    r.fog = blend(look.fog, Color{0x90, 0x98, 0xa0}, 0.5f * storm);
    r.cloud_tint = blend(look.cloud_tint, grey, storm);
    r.cloud_tint_amount = std::clamp(look.cloud_tint_amount + 0.7f * storm, 0.f, 1.f);
    r.fog_density = look.fog_density * (1.f + 1.8f * storm);
    r.sun_amount = look.sun_amount * (1.f - storm);
    r.haze = std::clamp(look.haze + 0.5f * storm, 0.f, 1.f);
    r.grip = std::clamp(look.grip - 0.3f * (r.rain - look.rain) - 0.3f * (r.snowfall - look.snowfall), 0.4f, 1.f);
    return r;
}


RoadTheme with_heavy_fog(const RoadTheme& look) {
    RoadTheme r = look;
    // Pea-soup: density must be high because fog uses (n/draw_distance)², so
    // modest values only haze the far third of the view. ~100 washes out by
    // ~10–15% of the draw distance.
    r.fog_density = std::max(look.fog_density * 8.f, 100.f);
    r.haze = 1.f; // mountains / hills dissolve into the air
    const Color mist{0xc0, 0xc8, 0xd0};
    const Color grey{0x88, 0x90, 0x98};
    r.fog = blend(look.fog, mist, 0.85f);
    r.sky_top = blend(look.sky_top, grey, 0.75f);
    r.sky_horizon = blend(look.sky_horizon, mist, 0.85f);
    r.cloud_tint = blend(look.cloud_tint, grey, 0.7f);
    r.cloud_tint_amount = std::clamp(look.cloud_tint_amount + 0.75f, 0.f, 1.f);
    r.sun_amount = look.sun_amount * 0.05f;
    // Soft drizzle only — not Stormy's downpour.
    r.rain = std::clamp(std::max(look.rain * 0.35f, 0.12f), 0.f, 0.4f);
    r.snowfall = std::clamp(look.snowfall * 0.4f, 0.f, 0.5f);
    r.grip = std::clamp(look.grip - 0.1f, 0.4f, 1.f);
    return r;
}

float lightning_rate(float rain) { return 0.25f * smoothstep(0.65f, 1.f, rain); }

} // namespace racer
