// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "framebuffer.hpp"
#include "track.hpp"

namespace racer {

// Day and night. A day passes in day_seconds of play; the hour (0 .. 24)
// gives how much light there is, the glow of dawn and dusk and the stars.
constexpr float day_seconds = 8.f * 60.f;
constexpr float start_hour = 10.f;

struct Daylight {
    float level = 1.f;  // 1 full day .. night_level
    float glow = 0.f;   // 0 .. 1, the red of sunrise and sunset
    float stars = 0.f;  // 0 .. 1
    float sun = 1.f;    // 0 .. 1, how much of the sun shows
};
constexpr float night_level = 0.22f;

Daylight daylight_at(float hour);

// The hour after `seconds` of play from `hour`, wrapped to 0 .. 24.
float advance_hour(float hour, float seconds);

// A zone's sky for the time of day: the colours of dawn and dusk, the sun
// low and red or gone, stars at night.
RoadTheme at_daytime(const RoadTheme& look, const Daylight& light);

// The picture for the light: darkened (warm at dusk, blue at night), except
// lamps and stars, which keep shining (night_emissive()).
void apply_daylight(Framebuffer& fb, const Daylight& light);
// Is this colour a light (tail and brake lights, indicators, headlights,
// the police lightbar, stars)?
bool night_emissive(Color c);

// The headlights' beam: the darkened road ahead lit again, the original
// picture `day` (before apply_daylight) blended back in, in a wedge from
// below row `bottom` towards the horizon at `horizon`, strongest at mid
// distance. Nothing to do in full daylight.
void headlight_beam(Framebuffer& fb, const std::vector<uint32_t>& day, const Daylight& light, int horizon,
                    int bottom);

} // namespace racer
