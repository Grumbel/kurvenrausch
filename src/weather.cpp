// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "weather.hpp"

#include <algorithm>
#include <cmath>

namespace racer {

namespace {

constexpr float margin = 12.f; // particles live a little outside the screen

constexpr Color rain_colour{0xc4, 0xd2, 0xe8};
constexpr Color snow_colour{0xff, 0xff, 0xff};

} // namespace

Weather::Weather(int width, int height)
    : w_(static_cast<float>(width)), h_(static_cast<float>(height)) {
    reset();
}

float Weather::next_random() {
    rng_ = rng_ * 1664525u + 1013904223u;
    return static_cast<float>(rng_ >> 8) / 16777216.f;
}

void Weather::reset() {
    rng_ = 0x51ed270bu;
    time_ = 0.f;
    rain_level_ = snow_level_ = 0.f;
    wind_ = 0.f;
    rain_.assign(static_cast<size_t>(max_rain), Particle{});
    snow_.assign(static_cast<size_t>(max_snow), Particle{});
    for (Particle& p : rain_) p = Particle{next_random() * (w_ + 2 * margin) - margin, next_random() * (h_ + 2 * margin) - margin, next_random(), 0.f};
    for (Particle& p : snow_) p = Particle{next_random() * (w_ + 2 * margin) - margin, next_random() * (h_ + 2 * margin) - margin, next_random(), next_random() * 6.28f};
}

void Weather::update(float rain, float snow, float wind, float dt) {
    rain_level_ = std::clamp(rain, 0.f, 1.f);
    snow_level_ = std::clamp(snow, 0.f, 1.f);
    wind_ = wind;
    time_ += dt;

    for (Particle& p : rain_) {
        p.y += (300.f + 380.f * p.depth) * dt;
        p.x += (wind_ - 45.f * (0.4f + p.depth)) * dt; // rain slants against the wind-free direction
        if (p.y > h_ + margin) { p.y -= h_ + 2 * margin; p.x = next_random() * (w_ + 2 * margin) - margin; }
        if (p.x < -margin) p.x += w_ + 2 * margin;
        if (p.x > w_ + margin) p.x -= w_ + 2 * margin;
    }
    for (Particle& p : snow_) {
        p.y += (20.f + 42.f * p.depth) * dt;
        p.x += (std::sin(time_ * 1.4f + p.phase) * (9.f + 12.f * p.depth) + wind_ * (0.3f + 0.7f * p.depth)) * dt;
        if (p.y > h_ + margin) { p.y -= h_ + 2 * margin; p.x = next_random() * (w_ + 2 * margin) - margin; }
        if (p.x < -margin) p.x += w_ + 2 * margin;
        if (p.x > w_ + margin) p.x -= w_ + 2 * margin;
    }
}

void Weather::render(Framebuffer& fb) const {
    for (int i = 0; i < rain_count(); ++i) {
        const Particle& p = rain_[static_cast<size_t>(i)];
        const int len = 3 + static_cast<int>(p.depth * 6.f);
        const float alpha = 0.3f + 0.4f * p.depth;
        const int x = static_cast<int>(std::floor(p.x)), y = static_cast<int>(std::floor(p.y));
        for (int k = 0; k < len; ++k) {
            // The streak leans against its direction of travel.
            const int sx = x + static_cast<int>(std::lround(static_cast<float>(k) * 0.35f));
            fb.blend_pixel(sx, y - k, rain_colour, alpha * (1.f - 0.5f * static_cast<float>(k) / static_cast<float>(len)));
        }
    }
    for (int i = 0; i < snow_count(); ++i) {
        const Particle& p = snow_[static_cast<size_t>(i)];
        const int x = static_cast<int>(std::floor(p.x)), y = static_cast<int>(std::floor(p.y));
        if (p.depth > 0.72f) {
            // Near flakes are 2x2 with a softer corner.
            fb.blend_pixel(x, y, snow_colour, 0.95f);
            fb.blend_pixel(x + 1, y, snow_colour, 0.8f);
            fb.blend_pixel(x, y + 1, snow_colour, 0.8f);
            fb.blend_pixel(x + 1, y + 1, snow_colour, 0.5f);
        } else {
            fb.blend_pixel(x, y, snow_colour, 0.55f + 0.45f * p.depth);
        }
    }
}

} // namespace racer
