// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include "framebuffer.hpp"

#include <cstdint>
#include <vector>

namespace racer {

// Rain streaks and snowflakes in screen space, drawn over the finished scene.
// Particles have a depth: near ones are larger, brighter and faster, which is
// what sells the third dimension. Everything is deterministic.
class Weather {
public:
    static constexpr int max_rain = 170;
    static constexpr int max_snow = 230;

    Weather(int width, int height);

    void reset();

    // rain and snow are intensities from 0 (none) to 1 (downpour / blizzard);
    // `wind` pushes the precipitation sideways in pixels per second.
    void update(float rain, float snow, float wind, float dt);

    void render(Framebuffer& fb) const;

    // Number of particles currently drawn, for tests.
    int rain_count() const { return static_cast<int>(rain_level_ * static_cast<float>(max_rain) + 0.5f); }
    int snow_count() const { return static_cast<int>(snow_level_ * static_cast<float>(max_snow) + 0.5f); }

private:
    struct Particle {
        float x, y;
        float depth;  // 0 far .. 1 near
        float phase;  // for the sway of snowflakes
    };

    float next_random();

    float w_, h_;

    std::vector<Particle> rain_;
    std::vector<Particle> snow_;
    float rain_level_ = 0.f;
    float snow_level_ = 0.f;
    float time_ = 0.f;
    float wind_ = 0.f;
    uint32_t rng_ = 0x51ed270bu;
};

} // namespace racer
