// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "weather.hpp"

#include <algorithm>
#include <cmath>

namespace racer {

namespace {

constexpr float margin = 12.f; // particles live a little outside the screen

// Outflow from the vanishing point: at top speed a particle moves away from
// it at this many times its distance per second (more for near ones).
constexpr float outflow_rate = 3.2f;
// How long a streak is: the distance travelled in this many seconds.
constexpr float streak_seconds = 0.03f;

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
    speed_ = 0.f;
    scatter();
}

void Weather::resize(int width, int height) {
    w_ = static_cast<float>(width);
    h_ = static_cast<float>(height);
    scatter();
}

void Weather::scatter() {
    rain_.assign(static_cast<size_t>(max_rain), Particle{});
    snow_.assign(static_cast<size_t>(max_snow), Particle{});
    for (Particle& p : rain_) p = Particle{next_random() * (w_ + 2 * margin) - margin, next_random() * (h_ + 2 * margin) - margin, next_random(), 0.f};
    for (Particle& p : snow_) p = Particle{next_random() * (w_ + 2 * margin) - margin, next_random() * (h_ + 2 * margin) - margin, next_random(), next_random() * 6.28f};
}

void Weather::move(Particle& p, float fall_x, float fall_y, float dt) {
    const float cx = w_ / 2.f, cy = h_ / 2.f; // vanishing point: centre, on the horizon
    const float k = outflow_rate * speed_ * (0.5f + p.depth);
    p.vx = fall_x + (p.x - cx) * k;
    p.vy = fall_y + (p.y - cy) * k;
    p.outflow = std::hypot(p.x - cx, p.y - cy) * k;
    p.x += p.vx * dt;
    p.y += p.vy * dt;

    const bool out = p.x < -margin || p.x > w_ + margin || p.y < -margin || p.y > h_ + margin;
    if (!out) return;
    // Back in: standing still, at the top (or wrapped sideways); the faster
    // the car, the more often near the vanishing point, where the stream
    // comes from.
    if (next_random() < 0.8f * std::min(speed_, 1.f)) {
        p.x = cx + (next_random() - 0.5f) * w_ * 0.7f;
        p.y = cy + (next_random() - 0.5f) * h_ * 0.7f;
    } else if (p.y > h_ + margin || p.y < -margin) {
        p.y = -margin + next_random() * margin;
        p.x = next_random() * (w_ + 2 * margin) - margin;
    } else {
        p.x = p.x < 0.f ? p.x + w_ + 2 * margin : p.x - w_ - 2 * margin;
    }
}

void Weather::update(float rain, float snow, float wind, float speed, float dt) {
    rain_level_ = std::clamp(rain, 0.f, 1.f);
    snow_level_ = std::clamp(snow, 0.f, 1.f);
    wind_ = wind;
    speed_ = std::max(0.f, speed);
    time_ += dt;

    for (Particle& p : rain_) {
        // Rain slants against the wind-free direction.
        move(p, wind_ - 45.f * (0.4f + p.depth), 300.f + 380.f * p.depth, dt);
    }
    for (Particle& p : snow_) {
        move(p, std::sin(time_ * 1.4f + p.phase) * (9.f + 12.f * p.depth) + wind_ * (0.3f + 0.7f * p.depth),
             20.f + 42.f * p.depth, dt);
    }
}

float Weather::mean_outflow() const {
    const float cx = w_ / 2.f, cy = h_ / 2.f;
    float sum = 0.f;
    int n = 0;
    auto add = [&](const std::vector<Particle>& ps, int count) {
        for (int i = 0; i < count; ++i) {
            const Particle& p = ps[static_cast<size_t>(i)];
            const float dx = p.x - cx, dy = p.y - cy;
            const float r = std::hypot(dx, dy);
            if (r < 1.f) continue;
            sum += (p.vx * dx + p.vy * dy) / r;
            ++n;
        }
    };
    add(rain_, rain_count());
    add(snow_, snow_count());
    return n ? sum / static_cast<float>(n) : 0.f;
}

void Weather::render(Framebuffer& fb) const {
    for (int i = 0; i < rain_count(); ++i) {
        const Particle& p = rain_[static_cast<size_t>(i)];
        // A streak trailing behind the drop along its motion, growing with
        // the outflow: at speed the rain streams out of the distance.
        const float v = std::hypot(p.vx, p.vy);
        const float len = std::min(3.f + p.depth * 6.f + p.outflow * streak_seconds, 12.f + p.depth * 32.f);
        const float alpha = 0.3f + 0.4f * p.depth;
        const float ux = v > 1e-3f ? p.vx / v : 0.f, uy = v > 1e-3f ? p.vy / v : 1.f;
        const int steps = static_cast<int>(len);
        for (int k = 0; k < steps; ++k) {
            const float t = static_cast<float>(k);
            fb.blend_pixel(static_cast<int>(std::floor(p.x - ux * t)), static_cast<int>(std::floor(p.y - uy * t)),
                           rain_colour, alpha * (1.f - 0.6f * t / len));
        }
    }
    for (int i = 0; i < snow_count(); ++i) {
        const Particle& p = snow_[static_cast<size_t>(i)];
        const int x = static_cast<int>(std::floor(p.x)), y = static_cast<int>(std::floor(p.y));
        // Flakes streaming past fast smear into short streaks behind them.
        const float v = std::hypot(p.vx, p.vy);
        const float len = std::min(p.outflow * streak_seconds * 0.6f, 4.f + 10.f * p.depth);
        if (len > 1.5f) {
            const float ux = p.vx / v, uy = p.vy / v;
            const float alpha = 0.55f + 0.45f * p.depth;
            for (int k = 0; k < static_cast<int>(len); ++k) {
                const float t = static_cast<float>(k);
                fb.blend_pixel(static_cast<int>(std::floor(p.x - ux * t)), static_cast<int>(std::floor(p.y - uy * t)),
                               snow_colour, alpha * (1.f - 0.7f * t / len));
            }
        } else if (p.depth > 0.72f) {
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
