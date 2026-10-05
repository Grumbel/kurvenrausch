// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "track.hpp"

#include <cstdint>

namespace racer {

// The weather passing over the lap: 0 clear .. 1 a storm, drifting with
// time. Every key_seconds there is a new target, picked by a hash (storms
// rarer than fair spells), eased into without a jump. Deterministic.
class WeatherFront {
public:
    static constexpr float key_seconds = 40.f;

    explicit WeatherFront(uint32_t seed = 0x5eed1234u) : seed_(seed) {}
    void update(float dt) { time_ += dt; }
    void reset() { time_ = 0.f; }
    // Hold the front at `level` (0 .. 1), or let it drift again (< 0).
    void force(float level) { forced_ = level; }
    float level() const;

private:
    float key(int index) const;

    uint32_t seed_;
    float time_ = 0.f;
    float forced_ = -1.f;
};

// A zone's look under a front at `level`: its rain and snow come and go
// around its own, a storm adds the climate's showers on top; wetter than
// usual, the sky and clouds darken, the fog thickens, the sun hides and the
// road gets slippery; drier, the road grips better.
RoadTheme weathered(const RoadTheme& look, float level);

// Dense fog without a full storm: thick distance fog, haze, muted sun and
// sky, only a light misty drizzle. Used for WeatherSetting::Foggy.
RoadTheme with_heavy_fog(const RoadTheme& look);

// Lightning: how many strikes per second a storm of rain `rain` brings.
float lightning_rate(float rain);

} // namespace racer
