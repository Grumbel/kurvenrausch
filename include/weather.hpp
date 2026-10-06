// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include "framebuffer.hpp"

#include <cstdint>
#include <vector>

namespace racer {

// Rain streaks and snowflakes in screen space, drawn over the finished scene.
// Particles have a depth: near ones are larger, brighter and faster, which is
// what sells the third dimension. Driving, they also stream outwards from the
// vanishing point, faster and in longer streaks the faster the car goes
// (vection). Everything is deterministic.
class Weather {
public:
    static constexpr int max_rain = 170;
    static constexpr int max_snow = 230;

    Weather(int width, int height);

    void reset();
    // A screen of another size: the drops and flakes spread over it anew.
    void resize(int width, int height);

    // rain and snow are intensities from 0 (none) to 1 (downpour / blizzard);
    // `wind` pushes the precipitation sideways in pixels per second; `speed`
    // is the car's speed as a fraction of its top speed.
    void update(float rain, float snow, float wind, float speed, float dt);

    // Mean speed in pixels per second at which the drawn particles move away
    // from the vanishing point, for tests.
    float mean_outflow() const;

    void render(Framebuffer& fb) const;

    // Number of particles currently drawn, for tests.
    int rain_count() const { return static_cast<int>(rain_level_ * static_cast<float>(max_rain) + 0.5f); }
    int snow_count() const { return static_cast<int>(snow_level_ * static_cast<float>(max_snow) + 0.5f); }

    // Screen-space particle for the GLES path (same layout as the internal
    // drops and flakes).
    struct Particle {
        float x, y;
        float depth;  // 0 far .. 1 near
        float phase;  // for the sway of snowflakes
        float vx = 0.f, vy = 0.f; // velocity at the last update, for the streaks
        float outflow = 0.f;      // the part of it streaming out of the distance, pixels per second
    };
    const Particle* rain_data() const { return rain_.data(); }
    const Particle* snow_data() const { return snow_.data(); }

private:

    float next_random();
    // Moves a particle by its fall `fall` (pixels per second) plus the outflow,
    // and puts it back in when it leaves the screen.
    void scatter(); // the drops and flakes anywhere on the screen
    void move(Particle& p, float fall_x, float fall_y, float dt);

    float w_, h_;

    std::vector<Particle> rain_;
    std::vector<Particle> snow_;
    float rain_level_ = 0.f;
    float snow_level_ = 0.f;
    float time_ = 0.f;
    float wind_ = 0.f;
    float speed_ = 0.f;
    uint32_t rng_ = 0x51ed270bu;
};

} // namespace racer
