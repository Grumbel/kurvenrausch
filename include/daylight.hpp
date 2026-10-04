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

// The night lit up by a place: towns and cities (glow up to 1) lighten the
// dark; out in the wilds (glow 0) it stays as dark as it is.
Daylight lit_by(Daylight light, float glow);

// A street lamp seen from the camera: its foot on screen column x, depth
// world units ahead; the pool of light it throws on the ground.
struct LampSpot {
    float x;
    float depth;
};
constexpr float lamp_reach = 1100.f; // the pool's radius, world units
// The pools of the lamps: the darkened ground around their feet lit again,
// warm, from the picture before nightfall (`day`), as headlight_beam() does.
void street_lights(Framebuffer& fb, const std::vector<uint32_t>& day, const Daylight& light,
                   const std::vector<float>& row_depth, const std::vector<LampSpot>& lamps, float camera_depth,
                   float x_scale);

// The picture for the light: darkened (warm at dusk, blue at night), except
// lamps and stars, which keep shining (night_emissive()).
void apply_daylight(Framebuffer& fb, const Daylight& light);
// Is this colour a light (tail and brake lights, indicators, headlights,
// the police lightbar, stars)?
bool night_emissive(Color c);

// Where the headlights shine, in the camera's world: the lamps `start`
// world units ahead of the camera, which looks straight down screen column
// `center`; turned by `aim` (world units sideways per unit ahead, with the
// steering). A row's ground is row_depth[y] ahead (0: none), and a world
// unit there is camera_depth / depth * x_scale pixels wide. Rows from
// `bottom` down are not lit (the car itself, the dashboard).
struct Beam {
    float start = 0.f;
    float center = 160.f;
    float aim = 0.f;
    float camera_depth = 1.f;
    float x_scale = 160.f;
    int bottom = 0;
};
constexpr float beam_reach = 6000.f;    // world units: half as bright this far from the lamps
constexpr float beam_half_width = 300.f; // at the lamps, world units ...
constexpr float beam_spread = 0.7f;      // ... widening by this per unit ahead (35 degrees)

// The headlights' beam: the darkened ground ahead lit again, the original
// picture `day` (before apply_daylight) blended back in: from the lamps on,
// widening ahead, fading with distance and to the sides, nothing over the
// horizon. Nothing to do in full daylight.
void headlight_beam(Framebuffer& fb, const std::vector<uint32_t>& day, const Daylight& light,
                    const std::vector<float>& row_depth, const Beam& beam);

} // namespace racer
