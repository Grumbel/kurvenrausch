// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "background.hpp"

#include "daylight.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <initializer_list>

namespace racer {

namespace {

constexpr int mountain_period = 1024; // pixels before the profile repeats
constexpr int hill_period = 768;
constexpr float sky_period = 1280.f;
constexpr float sky_height = 120.f; // pixels from the top of the sky gradient down to the horizon

// Scroll rates in pixels per segment travelled per unit of curve.
constexpr float sky_rate = 0.25f;
constexpr float mountain_rate = 0.5f;
constexpr float hill_rate = 1.f;

struct Wave {
    int cycles;  // whole cycles per period, so the profile tiles seamlessly
    float amplitude;
    float phase;
};

std::vector<float> profile(int period, std::initializer_list<Wave> waves, float lo, float hi) {
    std::vector<float> h(static_cast<size_t>(period));
    for (int x = 0; x < period; ++x) {
        float v = 0.f;
        for (const Wave& w : waves) {
            v += w.amplitude * std::sin(2.f * PI * static_cast<float>(w.cycles * x) /
                                        static_cast<float>(period) + w.phase);
        }
        h[static_cast<size_t>(x)] = v;
    }
    const auto [mn, mx] = std::minmax_element(h.begin(), h.end());
    const float a = *mn, b = *mx;
    for (float& v : h) v = lo + (v - a) / (b - a) * (hi - lo);
    return h;
}

Bitmap make_cloud(int w, int h, std::initializer_list<std::array<float, 4>> puffs, const RoadTheme& t) {
    Bitmap b(w, h);
    for (const auto& p : puffs) paint::shaded_ellipse(b, p[0], p[1], p[2], p[3], t.cloud[0], t.cloud[1], t.cloud[2]);
    // Flat bottoms, like cumulus.
    for (int y = h - 3; y < h; ++y)
        for (int x = 0; x < w; ++x) b.px[static_cast<size_t>(y) * w + x] = 0u;
    return b;
}

float wrap(float v, float period) {
    v = std::fmod(v, period);
    return v < 0.f ? v + period : v;
}

} // namespace

Background::Background() {
    // Broad harmonics only: fine cycles (was 23/41) broke slope lighting into
    // tiny lit/shade segments. Silhouette still varies; shading follows the
    // large ridges.
    mountains_ = profile(mountain_period,
                         {{2, 1.f, 0.3f}, {5, 0.6f, 1.7f}, {9, 0.3f, 0.4f}, {14, 0.12f, 2.2f}},
                         6.f, 52.f);
    hills_ = profile(hill_period, {{3, 1.f, 1.1f}, {6, 0.45f, 0.2f}, {10, 0.18f, 2.6f}}, 2.f, 20.f);

    const RoadTheme theme; // clouds are shaded with the default palette
    cloud_bitmaps_.push_back(make_cloud(56, 20, {{{14, 13, 10, 7}}, {{28, 9, 13, 9}}, {{42, 13, 11, 7}}}, theme));
    cloud_bitmaps_.push_back(make_cloud(36, 14, {{{11, 9, 8, 5}}, {{23, 7, 10, 6}}}, theme));
    cloud_bitmaps_.push_back(make_cloud(80, 22, {{{14, 15, 11, 6}}, {{32, 10, 15, 10}}, {{52, 12, 14, 8}}, {{68, 15, 10, 6}}}, theme));
    clouds_ = {{0, 40.f, 98.f}, {1, 230.f, 68.f}, {2, 410.f, 106.f}, {1, 640.f, 82.f}, {0, 860.f, 60.f}, {2, 1050.f, 90.f}};
}

void Background::reset() {
    sky_offset_ = mountain_offset_ = hill_offset_ = drift_ = 0.f;
}

void Background::update(float curve, float segments, float dt) {
    sky_offset_ = wrap(sky_offset_ + curve * segments * sky_rate, sky_period);
    mountain_offset_ = wrap(mountain_offset_ + curve * segments * mountain_rate, mountain_period);
    hill_offset_ = wrap(hill_offset_ + curve * segments * hill_rate, hill_period);
    drift_ = wrap(drift_ + dt * 3.f, sky_period); // clouds drift slowly on their own
}

void Background::render(Framebuffer& fb, const RoadTheme& theme, float hour) const {
    render(fb, theme, BackdropView{static_cast<float>(fb.height() / 2), 1.f, false}, hour);
}

void Background::render(Framebuffer& fb, const RoadTheme& theme, const BackdropView& view, float hour) const {
    // Extreme fog (FOGGY / white-out): no sky bands, mountains, clouds or sun —
    // the whole frame is air colour; the road pass draws the near field into it.
    if (theme.haze >= 0.99f || theme.fog_density >= 80.f) {
        fb.clear(atmosphere_air(theme));
        return;
    }

    const int w = fb.width();
    const int horizon = static_cast<int>(std::lround(view.horizon));
    const float zoom = view.zoom;
    const float half_w = static_cast<float>(w) / 2.f;
    // Position in a layer seen at screen column x (a pixel centre), for a
    // layer scrolled by `offset` that repeats every `period` pixels.
    auto layer_x = [&](float x, float offset, float period) {
        return view.mirror ? offset + period / 2.f - (x - half_w) / zoom : offset + x / zoom;
    };

    const SkyBody sun = sun_position(hour);
    const SkyBody moon = moon_position(hour);
    // Screen place of a body above the horizon; a little parallax from the
    // sky scroll so bends nudge it. Returns false when fully below.
    auto body_screen = [&](const SkyBody& body, float& sx, float& sy) {
        if (body.elevation < -0.08f) return false;
        const float elev = std::clamp(body.elevation, 0.f, 1.f);
        const float band = static_cast<float>(horizon) * 0.88f * zoom;
        sx = half_w + body.azimuth * half_w * 0.88f + (sky_offset_ * 0.04f) * zoom;
        sy = static_cast<float>(horizon) - elev * band;
        return true;
    };

    // Copper-style sky: 16 colour bands, dithered into each other. Near the
    // sun the horizon band warms a little for a more natural dawn/dusk wash.
    constexpr int bands = 16;
    const float sky_h = sky_height * zoom;
    const float sky_top = static_cast<float>(horizon) - sky_h; // above the screen in the mirror
    float sun_sx = 0.f, sun_sy = 0.f;
    const bool sun_up = !view.mirror && body_screen(sun, sun_sx, sun_sy);
    const bool sun_wash = sun_up && theme.sun_amount > 0.05f;
    uint32_t* fb_px = fb.pixels_mut();
    for (int y = 0; y < horizon; ++y) {
        const float t = std::max(0.f, static_cast<float>(y) - sky_top) / sky_h * (bands - 1);
        const int band = static_cast<int>(t);
        const float frac = t - static_cast<float>(band);
        Color c0 = blend(theme.sky_top, theme.sky_horizon, static_cast<float>(band) / (bands - 1));
        Color c1 = blend(theme.sky_top, theme.sky_horizon, static_cast<float>(std::min(band + 1, bands - 1)) / (bands - 1));
        if (sun_wash) {
            // Stronger warm haze near the sun, especially low on the horizon.
            const float near_h = std::clamp(1.f - std::abs(static_cast<float>(y) - sun_sy) / (28.f * zoom), 0.f, 1.f);
            const float low = std::clamp(1.f - sun.elevation, 0.f, 1.f);
            const float warm = 0.35f * near_h * low * theme.sun_amount;
            c0 = blend(c0, theme.sun, warm);
            c1 = blend(c1, theme.sun, warm);
        }
        const uint32_t a0 = c0.argb(), a1 = c1.argb();
        uint32_t* row = fb_px + static_cast<size_t>(y) * static_cast<size_t>(w);
        if (!sun_wash) {
            // Dither only: no per-pixel blend toward the sun.
            for (int x = 0; x < w; ++x) row[x] = bayer4(x, y) < frac ? a1 : a0;
            continue;
        }
        const float low = std::clamp(1.f - sun.elevation, 0.f, 1.f);
        const float inv = 1.f / (half_w * 0.5f);
        for (int x = 0; x < w; ++x) {
            Color c = bayer4(x, y) < frac ? c1 : c0;
            const float dx = (static_cast<float>(x) - sun_sx) * inv;
            const float near_x = std::clamp(1.f - dx * dx, 0.f, 1.f);
            c = blend(c, theme.sun, 0.12f * near_x * low * theme.sun_amount);
            row[x] = c.argb();
        }
    }
    fb.fill_rect(0, horizon, w, fb.height() - horizon, atmosphere_air(theme));

    // Stars at night, in a fixed field above the horizon (so the mirror
    // shows them too). They are lights: they keep shining in the
    // darkened picture (see apply_daylight()).
    if (theme.stars > 0.02f) {
        uint32_t seed = 0x51a7f00du;
        for (int i = 0; i < 90; ++i) {
            seed = seed * 1664525u + 1013904223u;
            const int x = static_cast<int>((seed >> 8) % static_cast<uint32_t>(w));
            seed = seed * 1664525u + 1013904223u;
            const int y = static_cast<int>(static_cast<float>((seed >> 8) % 1000u) / 1000.f * static_cast<float>(horizon) * 0.9f);
            const bool bright = (seed >> 28) < 4;
            if (static_cast<float>((seed >> 4) & 0xff) / 255.f > theme.stars) continue; // fewer at dusk
            fb.put_pixel(x, y, glowing(bright ? Color{0xe8, 0xee, 0xff} : Color{0xb8, 0xc8, 0xff}));
        }
    }

    // Sun: arc by the hour (ahead only; the mirror never shows it). Larger
    // and softer near the horizon, smaller near the zenith.
    if (theme.sun_amount > 0.02f && sun_up && sun.elevation > -0.02f) {
        const float low = std::clamp(1.f - sun.elevation, 0.f, 1.f);
        const float radius = (5.f + 4.f * theme.sun_amount + 6.f * low) * zoom;
        const float sx = sun_sx, sy = sun_sy;
        for (int y = static_cast<int>(sy - radius * 2.5f); y <= static_cast<int>(sy + radius * 2.5f); ++y) {
            if (y < 0 || y >= horizon) continue;
            for (int x = static_cast<int>(sx - radius * 2.5f); x <= static_cast<int>(sx + radius * 2.5f); ++x) {
                if (x < 0 || x >= w) continue;
                const float d = std::hypot(static_cast<float>(x) - sx, static_cast<float>(y) - sy);
                if (d <= radius) {
                    fb.put_pixel(x, y, blend(theme.sun, Color{255, 255, 255}, 0.3f * (1.f - d / radius)));
                } else if (d < radius * (1.8f + 0.6f * low)) {
                    const float glow = (1.f - (d - radius) / (radius * (0.8f + 0.6f * low))) * (0.4f + 0.35f * low) *
                                       theme.sun_amount;
                    if (bayer4(x, y) < glow) fb.put_pixel(x, y, blend(theme.sky_horizon, theme.sun, 0.55f));
                }
            }
        }
    }

    // Moon: opposite the sun, a pale disc when above the horizon. Visible at
    // night and faintly by day when the sun is low.
    float moon_sx = 0.f, moon_sy = 0.f;
    const bool moon_up = !view.mirror && body_screen(moon, moon_sx, moon_sy) && moon.elevation > 0.02f;
    const bool moon_show = moon_up && (theme.stars > 0.05f || sun.elevation < 0.25f);
    if (moon_show) {
        const float radius = 5.5f * zoom;
        const Color disc{0xe0, 0xe4, 0xec}, limb{0xb0, 0xb8, 0xc8};
        for (int y = static_cast<int>(moon_sy - radius * 1.6f); y <= static_cast<int>(moon_sy + radius * 1.6f); ++y) {
            if (y < 0 || y >= horizon) continue;
            for (int x = static_cast<int>(moon_sx - radius * 1.6f); x <= static_cast<int>(moon_sx + radius * 1.6f);
                 ++x) {
                if (x < 0 || x >= w) continue;
                const float d = std::hypot(static_cast<float>(x) - moon_sx, static_cast<float>(y) - moon_sy);
                if (d <= radius) {
                    // Slight shading on the left for a bit of form.
                    const float shade = std::clamp((static_cast<float>(x) - moon_sx) / radius * 0.5f + 0.5f, 0.f, 1.f);
                    fb.put_pixel(x, y, glowing(blend(limb, disc, shade)));
                } else if (d < radius * 1.45f && theme.stars > 0.2f) {
                    if (bayer4(x, y) < 0.2f * (1.f - (d - radius) / (0.45f * radius)))
                        fb.put_pixel(x, y, glowing(Color{0xc8, 0xd0, 0xe0}));
                }
            }
        }
    }

    for (const Cloud& c : clouds_) {
        const Bitmap& bmp = cloud_bitmaps_[static_cast<size_t>(c.bitmap)];
        const float bw = static_cast<float>(bmp.w);
        const float offset = sky_offset_ + drift_;
        // Left edge on screen, before and after the wrap of the sky layer.
        const float x = view.mirror ? half_w + (wrap(offset + sky_period / 2.f - c.x - bw, sky_period) - sky_period) * zoom
                                    : wrap(c.x - offset, sky_period) * zoom;
        for (float rep : {x, view.mirror ? x + sky_period * zoom : x - sky_period * zoom}) {
            if (rep + bw * zoom > 0.f && rep < static_cast<float>(w)) {
                fb.blit_scaled(bmp, std::floor(rep), static_cast<float>(horizon) - c.altitude * zoom,
                               bw * zoom, static_cast<float>(bmp.h) * zoom, view.mirror,
                               theme.cloud_tint_amount, theme.cloud_tint);
            }
        }
    }

    // Ridges are lit from the left: slopes rising to the left catch the light,
    // with a dithered transition between lit and shaded faces. Towards the
    // horizon they fade into the haze that also swallows the far road.
    auto ridge = [&](const std::vector<float>& h, float offset, Color lit, Color shade,
                     float snow_line, float scale) {
        const int period = static_cast<int>(h.size());
        for (int x = 0; x < w; ++x) {
            const int i = static_cast<int>(wrap(layer_x(static_cast<float>(x), offset, static_cast<float>(period)),
                                                static_cast<float>(period)));
            const float here = h[static_cast<size_t>(i)];
            // Slope over a wide window so lighting follows the broad ridge, not
            // column-to-column noise (period ~1k; ±32 samples ≈ 1/16 of a cycle
            // of the coarsest remaining harmonic).
            constexpr int slope_span = 32;
            const float slope = h[static_cast<size_t>((i + slope_span) % period)] -
                                h[static_cast<size_t>((i + period - slope_span) % period)];
            const float light = std::clamp(0.5f - slope * 0.04f, 0.f, 1.f);
            const int top = horizon - static_cast<int>(std::lround(here * scale * zoom));
            for (int y = top; y < horizon; ++y) {
                const float alt = static_cast<float>(horizon - y);
                Color c = bayer4(x, y) < light ? lit : shade;
                if (alt > (snow_line + 3.f * bayer4(x + 1, y)) * zoom) {
                    // Snow keeps the modelling of the slope underneath.
                    c = bayer4(x, y) < light ? theme.snow : blend(theme.snow, theme.mountain_shade, 0.5f);
                }
                const float haze = std::max(std::clamp(1.f - alt / (14.f * zoom), 0.f, 1.f) * 0.75f, theme.haze);
                fb.put_pixel(x, y, fogged_color(c, atmosphere_air(theme), haze));
            }
        }
    };
    ridge(mountains_, mountain_offset_, theme.mountain_lit, theme.mountain_shade, theme.snow_line, theme.mountain_scale);
    ridge(hills_, hill_offset_, theme.hill_lit, theme.hill_shade, 1.0e9f, theme.hill_scale);
}

std::vector<Background::CloudSprite> Background::cloud_sprites() const {
    std::vector<CloudSprite> out;
    out.reserve(clouds_.size());
    for (const Cloud& c : clouds_) {
        if (c.bitmap < 0 || static_cast<size_t>(c.bitmap) >= cloud_bitmaps_.size()) continue;
        out.push_back({&cloud_bitmaps_[static_cast<size_t>(c.bitmap)], c.x, c.altitude});
    }
    return out;
}

} // namespace racer
