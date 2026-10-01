// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "background.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <initializer_list>

namespace racer {

namespace {

constexpr int mountain_period = 1024; // pixels before the profile repeats
constexpr int hill_period = 768;
constexpr float sky_period = 1280.f;

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
    mountains_ = profile(mountain_period,
                         {{2, 1.f, 0.3f}, {5, 0.6f, 1.7f}, {11, 0.35f, 0.4f}, {23, 0.15f, 2.2f}, {41, 0.06f, 0.9f}},
                         6.f, 52.f);
    hills_ = profile(hill_period, {{3, 1.f, 1.1f}, {7, 0.5f, 0.2f}, {13, 0.2f, 2.6f}}, 2.f, 20.f);

    const RoadTheme theme; // clouds are shaded with the default palette
    cloud_bitmaps_.push_back(make_cloud(56, 20, {{{14, 13, 10, 7}}, {{28, 9, 13, 9}}, {{42, 13, 11, 7}}}, theme));
    cloud_bitmaps_.push_back(make_cloud(36, 14, {{{11, 9, 8, 5}}, {{23, 7, 10, 6}}}, theme));
    cloud_bitmaps_.push_back(make_cloud(80, 22, {{{14, 15, 11, 6}}, {{32, 10, 15, 10}}, {{52, 12, 14, 8}}, {{68, 15, 10, 6}}}, theme));
    clouds_ = {{0, 40.f, 22}, {1, 230.f, 52}, {2, 410.f, 14}, {1, 640.f, 38}, {0, 860.f, 60}, {2, 1050.f, 30}};
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

void Background::render(Framebuffer& fb, const RoadTheme& theme) const {
    const int w = fb.width();
    const int horizon = fb.height() / 2;

    // Copper-style sky: 16 colour bands, dithered into each other.
    constexpr int bands = 16;
    for (int y = 0; y < horizon; ++y) {
        const float t = static_cast<float>(y) / static_cast<float>(horizon) * (bands - 1);
        const int band = static_cast<int>(t);
        const float frac = t - static_cast<float>(band);
        const Color c0 = blend(theme.sky_top, theme.sky_horizon, static_cast<float>(band) / (bands - 1));
        const Color c1 = blend(theme.sky_top, theme.sky_horizon, static_cast<float>(std::min(band + 1, bands - 1)) / (bands - 1));
        for (int x = 0; x < w; ++x) fb.put_pixel(x, y, bayer4(x, y) < frac ? c1 : c0);
    }
    fb.fill_rect(0, horizon, w, fb.height() - horizon, theme.fog);

    // The sun sits in the sky layer, wrapping so it is seen most of the time.
    if (theme.sun_amount > 0.02f) {
        const float period = static_cast<float>(w) + 100.f;
        const float sx = wrap(215.f - sky_offset_, period) - 30.f;
        const float sy = static_cast<float>(horizon) - 30.f;
        const float radius = 7.f + 8.f * theme.sun_amount;
        for (int y = static_cast<int>(sy - radius * 2.f); y <= static_cast<int>(sy + radius * 2.f); ++y) {
            if (y >= horizon) break;
            for (int x = static_cast<int>(sx - radius * 2.f); x <= static_cast<int>(sx + radius * 2.f); ++x) {
                const float d = std::hypot(static_cast<float>(x) - sx, static_cast<float>(y) - sy);
                if (d <= radius) {
                    fb.put_pixel(x, y, blend(theme.sun, Color{255, 255, 255}, 0.25f * (1.f - d / radius)));
                } else if (d < radius * 2.f) {
                    // Dithered glow fading out into the sky.
                    const float glow = (1.f - (d - radius) / radius) * 0.55f * theme.sun_amount;
                    if (bayer4(x, y) < glow) fb.put_pixel(x, y, blend(theme.sky_horizon, theme.sun, 0.6f));
                }
            }
        }
    }

    for (const Cloud& c : clouds_) {
        const Bitmap& bmp = cloud_bitmaps_[static_cast<size_t>(c.bitmap)];
        const float x = wrap(c.x - sky_offset_ - drift_, sky_period);
        for (float rep : {x, x - sky_period}) {
            if (rep + static_cast<float>(bmp.w) > 0.f && rep < static_cast<float>(w)) {
                fb.blit_scaled(bmp, std::floor(rep), static_cast<float>(c.y),
                               static_cast<float>(bmp.w), static_cast<float>(bmp.h), false,
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
            const int i = static_cast<int>(wrap(static_cast<float>(x) + offset, static_cast<float>(period)));
            const float here = h[static_cast<size_t>(i)];
            // Slope over a wide window, so the lighting follows the broad shape of
            // the ridge instead of its fine wiggles.
            const float slope = h[static_cast<size_t>((i + 6) % period)] -
                                h[static_cast<size_t>((i + period - 6) % period)];
            const float light = std::clamp(0.5f - slope * 0.08f, 0.f, 1.f);
            const int top = horizon - static_cast<int>(std::lround(here * scale));
            for (int y = top; y < horizon; ++y) {
                const float alt = static_cast<float>(horizon - y);
                Color c = bayer4(x, y) < light ? lit : shade;
                if (alt > snow_line + 3.f * bayer4(x + 1, y)) {
                    // Snow keeps the modelling of the slope underneath.
                    c = bayer4(x, y) < light ? theme.snow : blend(theme.snow, theme.mountain_shade, 0.5f);
                }
                const float haze = std::max(std::clamp(1.f - alt / 14.f, 0.f, 1.f) * 0.75f, theme.haze);
                fb.put_pixel(x, y, blend(c, theme.fog, haze));
            }
        }
    };
    ridge(mountains_, mountain_offset_, theme.mountain_lit, theme.mountain_shade, theme.snow_line, theme.mountain_scale);
    ridge(hills_, hill_offset_, theme.hill_lit, theme.hill_shade, 1.0e9f, theme.hill_scale);
}

} // namespace racer
