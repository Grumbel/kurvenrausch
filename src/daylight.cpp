// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "daylight.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace racer {

namespace {

constexpr float pi = 3.14159265f;

float smoothstep(float a, float b, float x) {
    const float t = std::clamp((x - a) / (b - a), 0.f, 1.f);
    return t * t * (3.f - 2.f * t);
}

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
    // Cities lift the floor modestly; wilds keep the deep night_level.
    light.level += (1.f - light.level) * 0.32f * std::clamp(glow, 0.f, 1.f);
    return light;
}

RoadTheme at_daytime(const RoadTheme& look, const Daylight& light) {
    RoadTheme r = look;
    // Over a city the night sky glows orange from its lights.
    const float dark = std::clamp((1.f - light.level) / (1.f - night_level), 0.f, 1.f);
    r.sky_horizon = blend(look.sky_horizon, Color{0xe0, 0x90, 0x50}, 0.28f * dark * look.night_glow);
    r.sky_horizon = blend(look.sky_horizon, Color{0xff, 0x90, 0x50}, 0.7f * light.glow);
    r.sky_top = blend(look.sky_top, Color{0x60, 0x40, 0x90}, 0.4f * light.glow);
    r.sun = blend(look.sun, Color{0xff, 0x70, 0x30}, light.glow);
    // At dawn and dusk the sun shows even where the zone has none at noon.
    r.sun_amount = std::max(look.sun_amount, 0.6f * light.glow) * light.sun;
    r.cloud_tint = blend(look.cloud_tint, Color{0xc0, 0x50, 0x40}, light.glow);
    r.cloud_tint_amount = std::clamp(look.cloud_tint_amount + 0.5f * light.glow, 0.f, 1.f);
    // Light pollution: cities wash out the stars; open country keeps them bright.
    const float pollution = std::clamp(look.night_glow, 0.f, 1.f);
    const float clear = std::clamp(1.f - pollution, 0.f, 1.f);
    r.stars = light.stars * clear * clear; // quadratic: gone in city, full in the wilds
    return r;
}

void apply_daylight(Framebuffer& fb, const Daylight& light) {
    if (light.level >= 0.999f && light.glow <= 0.001f) return;
    // Per channel: blue at night, warm in the glow of dawn and dusk.
    const float night = (1.f - light.level) / (1.f - night_level);
    const float rf = light.level * (1.f - 0.15f * night) * (1.f - 0.05f * light.glow);
    const float gf = light.level * (1.f - 0.05f * night) * (1.f - 0.15f * light.glow);
    const float bf = light.level * (1.f + 0.45f * night) * (1.f - 0.30f * light.glow);
    // 8.8 fixed-point scales.
    const int r_scale = static_cast<int>(rf * 256.f + 0.5f);
    const int g_scale = static_cast<int>(gf * 256.f + 0.5f);
    const int b_scale = static_cast<int>(bf * 256.f + 0.5f);
    uint32_t* px = fb.pixels_mut();
    const int n = fb.width() * fb.height();
    for (int i = 0; i < n; ++i) {
        const uint32_t p = px[i];
        if (is_glowing(p)) continue; // a light
        int r = ((static_cast<int>((p >> 16) & 0xff) * r_scale) >> 8);
        int g = ((static_cast<int>((p >> 8) & 0xff) * g_scale) >> 8);
        int b = ((static_cast<int>(p & 0xff) * b_scale) >> 8);
        if (r > 255) r = 255;
        if (g > 255) g = 255;
        if (b > 255) b = 255;
        px[i] = 0xff000000u | static_cast<uint32_t>(r) << 16 | static_cast<uint32_t>(g) << 8 |
                static_cast<uint32_t>(b);
    }
}

void beam_rows(const Beam& beam, float dark, const std::vector<float>& row_depth, int height,
               std::vector<BeamRow>& rows) {
    const int h = std::min(height, static_cast<int>(row_depth.size()));
    rows.assign(static_cast<size_t>(std::max(0, height)), BeamRow{});
    if (dark <= 0.01f) return;
    // The cone at `depth` ahead, `air` its share (1 on the ground).
    auto cone = [&](float depth, float air) {
        BeamRow row;
        const float ahead = depth - beam.start; // from the lamps
        if (depth <= 0.f || ahead <= 0.f || ahead > 4.f * beam_reach) return row;
        // Bright from just past the lamps, fading with distance.
        const float reach = smoothstep(0.f, 200.f, ahead) / (1.f + (ahead / beam_reach) * (ahead / beam_reach));
        const float px_per_unit = beam.camera_depth / depth * beam.x_scale;
        row.half = (beam_half_width + beam_spread * ahead) * px_per_unit;
        // Player car is fixed at screen centre; the beam is camera-relative,
        // not road-centre-relative (that path was shifting cones beside the car).
        row.mid = beam.center + beam.aim * ahead * px_per_unit;
        row.k = 0.9f * dark * reach * air;
        return row;
    };
    // Bottom up, along the ground the beam lights, to its crest: where the
    // ground ends (sky above it), or where the next row shows ground much
    // further away than the slope so far would put it (the far side of a
    // hill, with air in between). Above the crest the beam goes on into the
    // air, over whatever the rows show: a glow easing from the crest's
    // brightness down to about half, fading out where the beam's far end
    // converges, at eye level (a crest at or above it fades within a quarter
    // of its height).
    const float horizon = beam.horizon >= 0.f ? beam.horizon : 0.5f * static_cast<float>(height);
    int crest_y = -1;
    float crest_depth = 0.f;
    bool past_crest = false;
    float inv1 = 0.f, inv2 = 0.f; // 1 / depth of the last two ground rows
    int run = 0;
    for (int y = std::min(beam.bottom, h) - 1; y >= 0; --y) {
        const float depth = row_depth[static_cast<size_t>(y)];
        BeamRow row = cone(depth, 1.f);
        if (!past_crest) {
            if (depth > 0.f) {
                // 1 / depth runs linearly up a stretch of road; a far smaller
                // value than its continuation means hidden ground in between.
                // Only where the beam still shows (far off it is gone anyway).
                const float inv = 1.f / depth;
                const float predicted = 2.f * inv1 - inv2;
                const BeamRow at_crest = crest_y >= 0 ? rows[static_cast<size_t>(crest_y)] : BeamRow{};
                if (run >= 2 && predicted > 0.f && inv < 0.5f * predicted && at_crest.k > 0.02f * dark) {
                    past_crest = true;
                } else {
                    crest_y = y;
                    crest_depth = depth;
                    inv2 = inv1;
                    inv1 = inv;
                    ++run;
                }
            } else if (crest_y >= 0) {
                past_crest = true;
            }
        }
        if (past_crest) {
            const float span = std::max(static_cast<float>(crest_y) - horizon, 0.25f * static_cast<float>(crest_y));
            const float t = static_cast<float>(crest_y - y) / std::max(1.f, span);
            if (t < 1.f) {
                const float glow = (1.f - 0.45f * smoothstep(0.f, 0.15f, t)) * (1.f - smoothstep(0.3f, 1.f, t));
                const BeamRow air = cone(crest_depth * (1.f + 1.8f * t), glow);
                if (air.k > row.k) row = air;
            }
        }
        rows[static_cast<size_t>(y)] = row;
    }
}

void headlight_beam(Framebuffer& fb, const std::vector<uint32_t>& day, const Daylight& light,
                    const std::vector<float>& row_depth, const Beam& beam,
                    const std::vector<float>& /*row_center_x*/) {
    const float dark = 1.f - light.level;
    if (dark <= 0.01f) return;
    uint32_t* px = fb.pixels_mut();
    const int w = fb.width();
    static thread_local std::vector<BeamRow> rows;
    beam_rows(beam, dark, row_depth, fb.height(), rows);
    for (int y = 0; y < static_cast<int>(rows.size()); ++y) {
        const BeamRow& row = rows[static_cast<size_t>(y)];
        if (row.k <= 0.f || row.half <= 0.f) continue;
        const float mid = row.mid, half = row.half;
        const int x0 = std::max(0, static_cast<int>(mid - half)), x1 = std::min(w, static_cast<int>(mid + half) + 1);
        for (int x = x0; x < x1; ++x) {
            const float across = std::abs(static_cast<float>(x) + 0.5f - mid) / half;
            if (across >= 1.f) continue;
            // Even across most of its width, a brighter spot in the middle,
            // soft at the sides.
            const float edge = smoothstep(1.f, 0.55f, across) * (0.75f + 0.25f * (1.f - across * across));
            const float k = row.k * edge;
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
                   float camera_depth, float x_scale, const std::vector<float>& row_center_x) {
    const float dark = 1.f - light.level;
    if (dark <= 0.01f || lamps.empty() || ground.size() != day.size()) return;
    uint32_t* px = fb.pixels_mut();
    const int w = fb.width(), h = std::min(fb.height(), static_cast<int>(row_depth.size()));
    const float screen_c = 0.5f * static_cast<float>(w);
    const bool have_rc = row_center_x.size() >= static_cast<size_t>(h);

    // World lateral offset of each lamp from the road centre at its depth, so
    // pools stay under the vehicle on every row (perspective + road curve).
    struct Placed {
        float world_off;
        float depth;
        float reach;
        Glow glow;
    };
    std::vector<Placed> placed;
    placed.reserve(lamps.size());
    for (const LampSpot& lamp : lamps) {
        if (lamp.depth <= 1e-3f) continue;
        float road_c = screen_c;
        if (have_rc) {
            float best = 1.0e9f;
            for (int yy = 0; yy < h; ++yy) {
                const float d = row_depth[static_cast<size_t>(yy)];
                if (d <= 0.f) continue;
                const float err = std::abs(d - lamp.depth);
                if (err < best) {
                    best = err;
                    road_c = row_center_x[static_cast<size_t>(yy)];
                }
            }
        }
        const float px_lamp = camera_depth / lamp.depth * x_scale;
        placed.push_back({(lamp.x - road_c) / px_lamp, lamp.depth, lamp.reach, lamp.glow});
    }

    for (int y = 0; y < h; ++y) {
        const float depth = row_depth[static_cast<size_t>(y)];
        if (depth <= 0.f) continue;
        const float px_per_unit = camera_depth / depth * x_scale;
        const float road_c = have_rc ? row_center_x[static_cast<size_t>(y)] : screen_c;
        for (const Placed& lamp : placed) {
            const float dz = depth - lamp.depth;
            if (std::abs(dz) >= lamp.reach) continue;
            const float cx = road_c + lamp.world_off * px_per_unit;
            // Sodium yellow, headlight white, tail light red; the red only a glow.
            const float tint_g = lamp.glow == Glow::Street ? 0.85f : lamp.glow == Glow::Tail ? 0.35f : 1.f;
            const float tint_b = lamp.glow == Glow::Street ? 0.55f : lamp.glow == Glow::Tail ? 0.2f : 0.95f;
            const float strength = lamp.glow == Glow::Tail ? 0.85f : lamp.glow == Glow::Head ? 0.8f : 0.9f;
            const float half = std::sqrt(lamp.reach * lamp.reach - dz * dz);
            const int x0 = std::max(0, static_cast<int>(cx - half * px_per_unit));
            const int x1 = std::min(w, static_cast<int>(cx + half * px_per_unit) + 1);
            for (int x = x0; x < x1; ++x) {
                const float dx = (static_cast<float>(x) + 0.5f - cx) / px_per_unit;
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
