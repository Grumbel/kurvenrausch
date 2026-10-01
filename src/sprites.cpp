// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "sprites.hpp"

#include "font.hpp"

#include <algorithm>
#include <cmath>
#include <string_view>

namespace racer {

namespace {

constexpr Color Outline{0x14, 0x18, 0x14};

Bitmap make_palm() {
    Bitmap b(80, 128);
    const Color bark_dark{0x5c, 0x3c, 0x20}, bark{0x8c, 0x64, 0x3a}, bark_light{0xb0, 0x88, 0x58};
    const Color leaf_dark{0x1a, 0x5a, 0x1e}, leaf{0x2a, 0x88, 0x2a}, leaf_light{0x6c, 0xc0, 0x48};

    // Gently leaning trunk with bark rings.
    auto trunk = [](float t) { return std::pair<float, float>{34.f + 10.f * t * t, 127.f - 92.f * t}; };
    constexpr int steps = 40;
    for (int i = 0; i < steps; ++i) {
        const float t0 = static_cast<float>(i) / steps, t1 = static_cast<float>(i + 1) / steps;
        auto [x0, y0] = trunk(t0);
        auto [x1, y1] = trunk(t1);
        const float w0 = 8.f - 3.5f * t0, w1 = 8.f - 3.5f * t1;
        paint::stroke(b, x0, y0, x1, y1, w0, w1, bark);
        paint::stroke(b, x0 + w0 * 0.3f, y0, x1 + w1 * 0.3f, y1, w0 * 0.35f, w1 * 0.35f, bark_dark);
        paint::stroke(b, x0 - w0 * 0.25f, y0, x1 - w1 * 0.25f, y1, 1.f, 1.f, bark_light);
        if (i % 3 == 0) paint::stroke(b, x0 - w0 / 2.f, y0, x0 + w0 / 2.f, y0, 1.f, 1.f, bark_dark);
    }

    // Fronds: drooping arcs with leaflets hanging off them.
    const float cx = 44.f, cy = 36.f;
    struct Frond { float angle_deg, length, droop; };
    const Frond fronds[] = {
        {192.f, 33.f, 18.f}, {218.f, 31.f, 14.f}, {248.f, 22.f, 9.f}, {280.f, 22.f, 10.f},
        {312.f, 30.f, 14.f}, {342.f, 32.f, 19.f}, {158.f, 26.f, 20.f}, {22.f, 26.f, 20.f},
        {120.f, 16.f, 14.f}, {60.f, 16.f, 14.f},
    };
    for (const Frond& f : fronds) {
        const float a = f.angle_deg * PI / 180.f;
        const float dx = std::cos(a), dy = std::sin(a);
        auto at = [&](float t) {
            return std::pair<float, float>{cx + dx * f.length * t, cy + dy * f.length * t + f.droop * t * t};
        };
        constexpr int n = 16;
        for (int i = 1; i < n; ++i) {
            const float t = static_cast<float>(i) / n;
            auto [x, y] = at(t);
            auto [xn, yn] = at(t + 1.f / n);
            float tx = xn - x, ty = yn - y;
            const float tl = std::hypot(tx, ty);
            tx /= tl; ty /= tl;
            // Normal pointing downwards
            float nx = -ty, ny = tx;
            if (ny < 0.f) { nx = -nx; ny = -ny; }
            const float len = 7.f * (1.f - t) + 2.f;
            paint::stroke(b, x, y, x + nx * len + tx * 2.f, y + ny * len + ty * 2.f, 1.5f, 1.f,
                          i % 2 ? leaf_dark : leaf);
            paint::stroke(b, x, y, x - nx * len * 0.4f + tx, y - ny * len * 0.4f + ty, 1.f, 1.f, leaf);
        }
        for (int i = 0; i < n; ++i) {
            auto [x0, y0] = at(static_cast<float>(i) / n);
            auto [x1, y1] = at(static_cast<float>(i + 1) / n);
            paint::stroke(b, x0, y0, x1, y1, 2.5f, 2.f, leaf_light);
        }
    }

    // Coconuts
    const Color nut_dark{0x40, 0x2c, 0x14}, nut{0x6c, 0x4c, 0x24}, nut_light{0x96, 0x70, 0x3c};
    paint::shaded_ellipse(b, cx - 3.f, cy + 3.f, 3.f, 3.f, nut_dark, nut, nut_light);
    paint::shaded_ellipse(b, cx + 3.f, cy + 4.f, 3.f, 3.f, nut_dark, nut, nut_light);
    paint::shaded_ellipse(b, cx, cy + 1.f, 3.f, 3.f, nut_dark, nut, nut_light);

    paint::outline(b, Outline);
    return b;
}

Bitmap make_tree() {
    Bitmap b(64, 80);
    const Color bark_dark{0x4a, 0x30, 0x1c}, bark{0x74, 0x50, 0x30};
    const Color dark{0x1c, 0x56, 0x1e}, mid{0x2e, 0x80, 0x2c}, light{0x5c, 0xb0, 0x40};

    paint::stroke(b, 32.f, 79.f, 32.f, 40.f, 9.f, 6.f, bark);
    paint::stroke(b, 34.f, 79.f, 34.f, 40.f, 3.f, 2.f, bark_dark);
    paint::stroke(b, 32.f, 52.f, 21.f, 38.f, 3.f, 2.f, bark);
    paint::stroke(b, 32.f, 48.f, 43.f, 36.f, 3.f, 2.f, bark);

    paint::shaded_ellipse(b, 32.f, 30.f, 26.f, 20.f, dark, mid, light);
    paint::shaded_ellipse(b, 17.f, 40.f, 14.f, 10.f, dark, mid, light);
    paint::shaded_ellipse(b, 47.f, 40.f, 14.f, 10.f, dark, mid, light);
    paint::shaded_ellipse(b, 23.f, 18.f, 14.f, 12.f, dark, mid, light);
    paint::shaded_ellipse(b, 42.f, 19.f, 14.f, 12.f, dark, mid, light);
    paint::shaded_ellipse(b, 32.f, 11.f, 13.f, 10.f, dark, mid, light);

    paint::outline(b, Outline);
    return b;
}

Bitmap make_bush() {
    Bitmap b(48, 24);
    const Color dark{0x20, 0x5a, 0x20}, mid{0x34, 0x86, 0x30}, light{0x68, 0xb4, 0x48};
    paint::shaded_ellipse(b, 12.f, 16.f, 11.f, 9.f, dark, mid, light);
    paint::shaded_ellipse(b, 36.f, 16.f, 11.f, 9.f, dark, mid, light);
    paint::shaded_ellipse(b, 24.f, 12.f, 13.f, 11.f, dark, mid, light);
    const Color blossom{0xf0, 0x70, 0xb0};
    const int dots[][2] = {{10, 12}, {18, 7}, {27, 5}, {33, 11}, {40, 14}, {22, 15}, {30, 18}};
    for (const auto& d : dots) b.set(d[0], d[1], blossom);
    paint::outline(b, Outline);
    return b;
}

Bitmap make_boulder(Color dark, Color mid, Color light) {
    Bitmap b(48, 32);
    paint::shaded_ellipse(b, 22.f, 19.f, 20.f, 13.f, dark, mid, light);
    paint::shaded_ellipse(b, 37.f, 24.f, 10.f, 8.f, dark, mid, light);
    paint::stroke(b, 18.f, 10.f, 23.f, 18.f, 1.f, 1.f, dark);
    paint::stroke(b, 23.f, 18.f, 20.f, 25.f, 1.f, 1.f, dark);
    paint::outline(b, Outline);
    return b;
}

Bitmap make_billboard(std::string_view line1, std::string_view line2,
                      Color top, Color bottom, Color ink_colour) {
    Bitmap b(96, 64);
    const Color post{0x6a, 0x6a, 0x70}, post_dark{0x44, 0x44, 0x4a};
    paint::rect(b, 14, 40, 6, 24, post);
    paint::rect(b, 18, 40, 2, 24, post_dark);
    paint::rect(b, 76, 40, 6, 24, post);
    paint::rect(b, 80, 40, 2, 24, post_dark);

    paint::rect(b, 0, 0, 96, 42, Color{0xf0, 0xf0, 0xf0});
    for (int y = 3; y < 39; ++y) {
        const float t = static_cast<float>(y - 3) / 36.f;
        paint::rect(b, 3, y, 90, 1, blend(top, bottom, t));
    }
    const Color shadow{0x10, 0x08, 0x20};
    const int x1 = (96 - font::text_width(line1, 2)) / 2;
    const int x2 = (96 - font::text_width(line2, 2)) / 2;
    paint::text(b, x1 + 1, 7, line1, shadow, 2);
    paint::text(b, x1, 6, line1, ink_colour, 2);
    paint::text(b, x2 + 1, 23, line2, shadow, 2);
    paint::text(b, x2, 22, line2, ink_colour, 2);

    paint::outline(b, Outline);
    return b;
}

Bitmap make_gantry() {
    Bitmap b(208, 80);
    const Color red{0xc8, 0x18, 0x18}, white{0xf4, 0xf4, 0xf4}, black{0x18, 0x18, 0x18};
    for (int y = 0; y < 80; y += 8) {
        paint::rect(b, 0, y, 12, 8, (y / 8) % 2 ? red : white);
        paint::rect(b, 196, y, 12, 8, (y / 8) % 2 ? red : white);
    }
    paint::rect(b, 12, 4, 184, 32, red);
    for (int x = 12; x < 196; x += 4) {
        const bool odd = ((x - 12) / 4) % 2;
        paint::rect(b, x, 4, 4, 4, odd ? black : white);
        paint::rect(b, x, 8, 4, 4, odd ? white : black);
        paint::rect(b, x, 28, 4, 4, odd ? black : white);
        paint::rect(b, x, 32, 4, 4, odd ? white : black);
    }
    const int x = (208 - font::text_width("START", 2)) / 2;
    paint::text(b, x + 1, 15, "START", black, 2);
    paint::text(b, x, 14, "START", white, 2);
    paint::outline(b, Outline);
    return b;
}


// Shades the left edge of every opaque run lighter and the right edge darker,
// which turns flat silhouettes into rounded forms lit from the left.
void edge_shade(Bitmap& b, Color light, Color dark, int width = 1) {
    const Bitmap src = b;
    for (int y = 0; y < b.h; ++y) {
        for (int x = 0; x < b.w; ++x) {
            if (!src.opaque(x, y)) continue;
            bool left = true, right = true;
            for (int k = 1; k <= width; ++k) {
                left = left && !src.opaque(x - k, y);
                right = right && !src.opaque(x + k, y);
            }
            for (int k = 1; k < width; ++k) {
                if (!src.opaque(x - k, y)) left = true;
                if (!src.opaque(x + k, y)) right = true;
            }
            if (left) b.set(x, y, light);
            else if (right) b.set(x, y, dark);
        }
    }
}

// One tier of a conifer: a triangle with a toothed lower edge, lit from the
// left. With snow the upper part of the tier is white.
void conifer_tier(Bitmap& b, int cx, int top, int bottom, int half_w, bool snowy,
                  Color dark, Color mid, Color light) {
    const Color snow_lit{0xf6, 0xfa, 0xff}, snow_shade{0xb4, 0xc6, 0xe2};
    for (int y = top; y <= bottom; ++y) {
        const float t = static_cast<float>(y - top) / static_cast<float>(bottom - top);
        const float hw = static_cast<float>(half_w) * t + 0.5f;
        for (int x = cx - half_w; x <= cx + half_w; ++x) {
            const float u = (static_cast<float>(x - cx)) / (hw + 0.01f);
            if (std::abs(u) > 1.f) continue;
            if (y >= bottom - 1 && ((x >> 1) & 1)) continue; // toothed lower edge
            // The upper half of a tier is hidden under the next tier, so the snow
            // band extends well down into the visible part.
            if (snowy && t < 0.84f - 0.38f * std::abs(u)) {
                b.set(x, y, u > 0.1f + 0.3f * (bayer4(x, y) - 0.5f) ? snow_shade : snow_lit);
                continue;
            }
            const float v = 0.5f - 0.5f * u - 0.22f * t + (bayer4(x, y) - 0.5f) * 0.4f;
            b.set(x, y, v > 0.55f ? light : v > 0.18f ? mid : dark);
        }
    }
}

Bitmap make_fir(bool snowy) {
    Bitmap b(56, 100);
    const Color bark_dark{0x4a, 0x30, 0x1c}, bark{0x74, 0x50, 0x30};
    paint::rect(b, 25, 86, 7, 14, bark);
    paint::rect(b, 29, 86, 3, 14, bark_dark);
    const Color dark{0x12, 0x40, 0x22}, mid{0x1e, 0x62, 0x30}, light{0x3a, 0x88, 0x44};
    // Bottom tier first, so each higher tier overlaps the one below it.
    conifer_tier(b, 28, 62, 92, 27, snowy, dark, mid, light);
    conifer_tier(b, 28, 46, 78, 23, snowy, dark, mid, light);
    conifer_tier(b, 28, 30, 62, 19, snowy, dark, mid, light);
    conifer_tier(b, 28, 15, 46, 15, snowy, dark, mid, light);
    conifer_tier(b, 28, 1, 28, 10, snowy, dark, mid, light);
    paint::outline(b, Outline);
    return b;
}

Bitmap make_cactus() {
    Bitmap b(40, 72);
    const Color mid{0x3a, 0x8a, 0x3c}, light{0x70, 0xc2, 0x5a}, dark{0x20, 0x5c, 0x2c};
    // Trunk with a rounded top, two upturned arms.
    paint::stroke(b, 20.f, 70.f, 20.f, 9.f, 11.f, 11.f, mid);
    paint::ellipse(b, 20.f, 9.f, 5.5f, 5.5f, mid);
    paint::stroke(b, 20.f, 46.f, 8.f, 46.f, 7.f, 7.f, mid);
    paint::stroke(b, 8.f, 46.f, 8.f, 26.f, 7.f, 7.f, mid);
    paint::ellipse(b, 8.f, 25.f, 3.5f, 3.5f, mid);
    paint::stroke(b, 20.f, 38.f, 32.f, 38.f, 7.f, 7.f, mid);
    paint::stroke(b, 32.f, 38.f, 32.f, 19.f, 7.f, 7.f, mid);
    paint::ellipse(b, 32.f, 18.f, 3.5f, 3.5f, mid);
    edge_shade(b, light, dark, 2);
    // Vertical ribs.
    for (int x : {17, 23}) {
        for (int y = 12; y < 70; ++y) {
            if (b.opaque(x, y) && hash01(x, y) > 0.25f) b.set(x, y, dark);
        }
    }
    for (int x : {8, 32}) {
        for (int y : {22, 28, 34}) b.set(x, y, dark);
    }
    b.set(20, 4, Color{0xf0, 0x70, 0xb0}); // a blossom on top
    b.set(21, 4, Color{0xf8, 0xa0, 0xc8});
    paint::outline(b, Outline);
    return b;
}

Bitmap make_mesa() {
    Bitmap b(192, 72);
    const Color dark{0x8a, 0x3a, 0x24}, mid{0xc2, 0x58, 0x32}, light{0xe2, 0x86, 0x52};
    const Color cap{0x6a, 0x30, 0x20}, talus_a{0xd8, 0x9a, 0x6c}, talus_b{0xb8, 0x78, 0x50};
    for (int x = 0; x < b.w; ++x) {
        // Cliff band between x = 34 and 158, widening into a talus slope below.
        const float jag = hash01(x / 3, 7) * 3.f + hash01(x / 11, 3) * 4.f;
        int cliff_top = 12 + static_cast<int>(jag);
        if (x < 40) cliff_top += (40 - x) / 2;       // shoulders drop away at both ends
        if (x > 152) cliff_top += (x - 152) / 2;
        const int cliff_bottom = 46;
        const float fx = static_cast<float>(x - 34) / 124.f;
        if (x >= 34 && x <= 158) {
            for (int y = cliff_top; y < cliff_bottom; ++y) {
                const int strata = (y + static_cast<int>(hash01(x / 17, 1) * 5.f)) / 5;
                Color c = strata % 3 == 0 ? mid : strata % 3 == 1 ? light : mid;
                if (y < cliff_top + 4) c = cap; // dark cap rock on top
                // Lit from the left, shaded to the right, with gullies.
                if (fx > 0.55f + 0.2f * (bayer4(x, y) - 0.5f)) c = blend(c, dark, 0.55f);
                if (fx < 0.18f + 0.2f * (bayer4(x, y) - 0.5f) && y >= cliff_top + 4) c = blend(c, light, 0.4f);
                if (hash01(x / 6, 11) > 0.82f && y > cliff_top + 6) c = blend(c, dark, 0.5f);
                b.set(x, y, c);
            }
        }
        // Talus slope: a trapezoid from y = 42 (x 30..162) down to y = 71 (x 4..188).
        for (int y = 42; y < b.h; ++y) {
            const float t = static_cast<float>(y - 42) / 29.f;
            const float left = 30.f - 26.f * t, right = 162.f + 26.f * t;
            if (static_cast<float>(x) < left || static_cast<float>(x) > right) continue;
            const bool shaded = static_cast<float>(x) > 100.f + 40.f * (bayer4(x, y) - 0.5f);
            Color c = shaded ? talus_b : talus_a;
            if (hash01(x / 4, 5) > 0.8f) c = blend(c, dark, 0.35f); // erosion gullies
            b.set(x, y, c);
        }
    }
    paint::outline(b, Outline);
    return b;
}

Bitmap make_cypress() {
    Bitmap b(28, 100);
    paint::rect(b, 12, 90, 4, 10, Color{0x6c, 0x48, 0x28});
    const Color dark{0x12, 0x3e, 0x22}, mid{0x22, 0x66, 0x34}, light{0x48, 0x90, 0x4c};
    const struct { float y, rx, ry; } puffs[] = {
        {80.f, 7.f, 11.f}, {68.f, 7.5f, 11.f}, {56.f, 7.5f, 11.f}, {44.f, 7.5f, 11.f},
        {32.f, 7.f, 11.f}, {21.f, 5.5f, 10.f}, {11.f, 3.5f, 9.f},
    };
    for (const auto& p : puffs) paint::shaded_ellipse(b, 14.f, p.y, p.rx, p.ry, dark, mid, light);
    paint::outline(b, Outline);
    return b;
}

Bitmap make_chalet() {
    Bitmap b(88, 64);
    const Color wood_a{0x8c, 0x5c, 0x32}, wood_b{0x74, 0x48, 0x26}, stone{0x8c, 0x88, 0x80};
    const Color roof_a{0x5c, 0x3a, 0x24}, roof_b{0x48, 0x2c, 0x1a};
    const Color snow_lit{0xf6, 0xfa, 0xff}, snow_shade{0xc0, 0xd0, 0xe8};
    // Walls with horizontal planks and a stone base.
    for (int y = 30; y < 62; ++y) {
        const Color row = (y / 3) % 2 ? wood_a : wood_b;
        paint::rect(b, 14, y, 60, 1, y >= 56 ? stone : row);
    }
    // Gable roof with a wide overhang, snow on the upper half.
    for (int y = 4; y < 31; ++y) {
        const float hw = 8.f + static_cast<float>(y - 4) * (38.f / 26.f);
        const int x0 = 44 - static_cast<int>(hw), x1 = 44 + static_cast<int>(hw);
        for (int x = x0; x <= x1; ++x) {
            Color c = (y / 2) % 2 ? roof_a : roof_b;
            const float depth = static_cast<float>(std::min(x - x0, x1 - x));
            if (static_cast<float>(y - 4) < 14.f + 2.f * (bayer4(x, y) - 0.5f) && depth > 1.f) {
                c = x < 44 ? snow_lit : snow_shade;
            }
            b.set(x, y, c);
        }
    }
    // Chimney, balcony, windows with shutters and flower boxes, door.
    paint::rect(b, 60, 6, 6, 12, stone);
    paint::rect(b, 59, 5, 8, 2, Color{0xe8, 0xe8, 0xe8});
    paint::rect(b, 16, 39, 56, 2, wood_b);
    for (int x = 17; x < 72; x += 4) paint::rect(b, x, 41, 1, 5, wood_b);
    for (int wx : {20, 58}) {
        paint::rect(b, wx - 3, 44, 3, 11, Color{0x2c, 0x78, 0x3c});
        paint::rect(b, wx + 10, 44, 3, 11, Color{0x2c, 0x78, 0x3c});
        paint::rect(b, wx, 44, 10, 11, Color{0x90, 0xc4, 0xe8});
        paint::rect(b, wx + 4, 44, 1, 11, Color{0xe8, 0xe8, 0xe8});
        paint::rect(b, wx, 49, 10, 1, Color{0xe8, 0xe8, 0xe8});
        paint::rect(b, wx - 1, 55, 12, 3, wood_b);
        for (int fx = wx; fx < wx + 10; fx += 2) b.set(fx, 54, (fx / 2) % 2 ? Color{0xe0, 0x30, 0x40} : Color{0xf0, 0x80, 0xa0});
    }
    paint::rect(b, 39, 46, 10, 16, Color{0x4a, 0x2c, 0x18});
    paint::rect(b, 40, 47, 8, 14, Color{0x5c, 0x38, 0x20});
    b.set(46, 55, Color{0xf0, 0xd0, 0x50});
    paint::outline(b, Outline);
    return b;
}

Bitmap make_pole() {
    Bitmap b(20, 160);
    const Color wood{0x7a, 0x56, 0x34}, light{0xa0, 0x78, 0x4c}, dark{0x4e, 0x34, 0x20};
    paint::stroke(b, 10.f, 159.f, 10.f, 6.f, 5.f, 3.f, wood);
    paint::rect(b, 1, 14, 18, 3, wood);
    paint::rect(b, 4, 27, 12, 2, wood);
    edge_shade(b, light, dark, 1);
    for (int y = 30; y < 160; ++y) if (hash01(y, 2) > 0.85f) b.set(10, y, dark); // cracks
    for (int x : {2, 17}) { b.set(x, 12, Color{0x7c, 0xc4, 0xd0}); b.set(x, 13, Color{0x7c, 0xc4, 0xd0}); }
    for (int x : {5, 14}) { b.set(x, 25, Color{0x7c, 0xc4, 0xd0}); b.set(x, 26, Color{0x7c, 0xc4, 0xd0}); }
    paint::outline(b, Outline);
    return b;
}

Bitmap make_dry_shrub() {
    Bitmap b(40, 24);
    const Color dark{0x70, 0x54, 0x2c}, mid{0xa8, 0x88, 0x4c}, light{0xd8, 0xbc, 0x7c};
    paint::shaded_ellipse(b, 20.f, 13.f, 17.f, 10.f, dark, mid, light);
    // Twiggy look: dark strokes across, and holes where light shines through.
    for (int i = 0; i < 16; ++i) {
        const float x0 = 6.f + hash01(i, 1) * 28.f, y0 = 5.f + hash01(i, 2) * 16.f;
        const float x1 = 6.f + hash01(i, 3) * 28.f, y1 = 5.f + hash01(i, 4) * 16.f;
        paint::stroke(b, x0, y0, x1, y1, 1.f, 1.f, i % 3 ? dark : light);
    }
    for (int y = 0; y < b.h; ++y)
        for (int x = 0; x < b.w; ++x)
            if (b.opaque(x, y) && hash01(x, y + 40) < 0.14f) b.px[static_cast<size_t>(y) * b.w + x] = 0u;
    paint::outline(b, Outline);
    return b;
}

} // namespace

Bitmap make_car(const CarStyle& style, int turn) {
    Bitmap b(96, 44);
    const Color tire{0x18, 0x18, 0x1c}, tread{0x3c, 0x3c, 0x42};
    const Color chrome{0x9a, 0x9a, 0xa8}, grille{0x14, 0x14, 0x18}, slat{0x3a, 0x3a, 0x42};
    const Color lamp{0xf0, 0x28, 0x1c}, lamp_hi{0xff, 0x9a, 0x78};

    // Shadow and tyres stay planted while the body shifts into the turn.
    paint::ellipse(b, 48.f, 41.f, 46.f, 3.f, Color{0x22, 0x22, 0x22});
    for (int side = 0; side < 2; ++side) {
        const int x = side ? 78 : 4;
        paint::rect(b, x, 27, 14, 15, tire);
        for (int y = 28; y < 41; y += 3) paint::rect(b, x + 2, y, 10, 1, tread);
    }

    const int s = turn; // body lean
    paint::ellipse(b, 15.f + s, 27.f, 9.f, 9.f, style.body);
    paint::ellipse(b, 81.f + s, 27.f, 9.f, 9.f, style.body);
    paint::rect(b, 8 + s, 19, 80, 18, style.body);
    paint::rect(b, 10 + s, 15, 76, 5, style.body_light);
    paint::rect(b, 12 + s, 14, 72, 1, style.body_light);
    paint::rect(b, 9 + s, 32, 78, 5, style.body_dark);
    paint::rect(b, 12 + s, 36, 72, 3, Color{0x4c, 0x4c, 0x54});

    paint::rect(b, 12 + s, 22, 20, 6, lamp);
    paint::rect(b, 64 + s, 22, 20, 6, lamp);
    paint::rect(b, 13 + s, 23, 18, 1, lamp_hi);
    paint::rect(b, 65 + s, 23, 18, 1, lamp_hi);
    paint::rect(b, 34 + s, 22, 28, 9, grille);
    for (int y = 23; y < 31; y += 2) paint::rect(b, 35 + s, y, 26, 1, slat);
    paint::rect(b, 40 + s, 31, 16, 4, Color{0xe8, 0xe8, 0xd8});
    paint::rect(b, 43 + s, 32, 10, 1, Color{0x30, 0x30, 0x60});
    paint::ellipse(b, 24.f + s, 38.5f, 2.5f, 1.5f, Color{0x30, 0x30, 0x34});
    paint::ellipse(b, 72.f + s, 38.5f, 2.5f, 1.5f, Color{0x30, 0x30, 0x34});

    const int u = 2 * turn; // cabin leans further than the body
    if (style.convertible) {
        const Color hair_dark{0x3a, 0x22, 0x14}, hair{0x5c, 0x38, 0x1c}, hair_light{0x84, 0x58, 0x2c};
        const Color blond_dark{0xb0, 0x80, 0x30}, blond{0xe0, 0xb4, 0x50}, blond_light{0xf8, 0xe0, 0x90};
        const Color rest_dark{0x20, 0x20, 0x24}, rest{0x3c, 0x3c, 0x44}, rest_light{0x60, 0x60, 0x6a};
        paint::shaded_ellipse(b, 60.f + u, 8.f, 6.f, 7.f, blond_dark, blond, blond_light);
        paint::shaded_ellipse(b, 36.f + u, 6.f, 5.f, 5.f, hair_dark, hair, hair_light);
        paint::shaded_ellipse(b, 36.f + u, 12.f, 6.f, 4.f, rest_dark, rest, rest_light);
        paint::shaded_ellipse(b, 60.f + u, 12.f, 6.f, 4.f, rest_dark, rest, rest_light);
        paint::stroke(b, 21.f + u, 15.f, 28.f + u, 7.f, 1.5f, 1.5f, chrome);
        paint::stroke(b, 75.f + u, 15.f, 68.f + u, 7.f, 1.5f, 1.5f, chrome);
        paint::stroke(b, 28.f + u, 7.f, 68.f + u, 7.f, 1.f, 1.f, chrome);
    } else {
        paint::rect(b, 22 + u, 3, 52, 12, style.body);
        paint::rect(b, 24 + u, 2, 48, 1, style.body_light);
        paint::rect(b, 26 + u, 5, 44, 8, Color{0x2c, 0x3c, 0x54});
        paint::stroke(b, 30.f + u, 12.f, 36.f + u, 5.f, 1.f, 1.f, Color{0x70, 0x88, 0xa8});
    }

    paint::outline(b, Outline);
    return b;
}

SpriteSheet::SpriteSheet() {
    scenery_[static_cast<size_t>(Scenery::Palm)] = make_palm();
    scenery_[static_cast<size_t>(Scenery::Tree)] = make_tree();
    scenery_[static_cast<size_t>(Scenery::Bush)] = make_bush();
    scenery_[static_cast<size_t>(Scenery::Boulder)] =
        make_boulder(Color{0x58, 0x50, 0x48}, Color{0x8a, 0x82, 0x74}, Color{0xb6, 0xae, 0x9c});
    scenery_[static_cast<size_t>(Scenery::Billboard)] =
        make_billboard("KURVEN", "RAUSCH", Color{0x18, 0x30, 0xa0}, Color{0x70, 0x18, 0x90}, Color{0xff, 0xd8, 0x20});
    scenery_[static_cast<size_t>(Scenery::Gantry)] = make_gantry();
    scenery_[static_cast<size_t>(Scenery::Fir)] = make_fir(false);
    scenery_[static_cast<size_t>(Scenery::SnowFir)] = make_fir(true);
    scenery_[static_cast<size_t>(Scenery::Cactus)] = make_cactus();
    scenery_[static_cast<size_t>(Scenery::Mesa)] = make_mesa();
    scenery_[static_cast<size_t>(Scenery::Cypress)] = make_cypress();
    scenery_[static_cast<size_t>(Scenery::Chalet)] = make_chalet();
    scenery_[static_cast<size_t>(Scenery::RedRock)] =
        make_boulder(Color{0x7a, 0x34, 0x24}, Color{0xb4, 0x56, 0x34}, Color{0xdc, 0x84, 0x58});
    scenery_[static_cast<size_t>(Scenery::Pole)] = make_pole();
    scenery_[static_cast<size_t>(Scenery::DryShrub)] = make_dry_shrub();
    scenery_[static_cast<size_t>(Scenery::BillboardUs)] =
        make_billboard("ROUTE", " 66 ", Color{0xb0, 0x18, 0x28}, Color{0x1c, 0x2c, 0x8c}, Color{0xff, 0xff, 0xff});

    const CarStyle player{{0x88, 0x08, 0x10}, {0xd0, 0x18, 0x1c}, {0xf0, 0x60, 0x50}, true};
    for (int turn = -1; turn <= 1; ++turn) {
        player_[static_cast<size_t>(turn + 1)] = make_car(player, turn);
    }

    const CarStyle traffic[traffic_styles] = {
        {{0x14, 0x2c, 0x80}, {0x24, 0x50, 0xc8}, {0x70, 0x98, 0xf0}, false},
        {{0xa0, 0x80, 0x10}, {0xe8, 0xc0, 0x20}, {0xf8, 0xe8, 0x80}, false},
        {{0x98, 0x98, 0xa0}, {0xd8, 0xd8, 0xe0}, {0xf8, 0xf8, 0xff}, false},
        {{0x14, 0x5c, 0x30}, {0x24, 0x8c, 0x48}, {0x70, 0xc8, 0x88}, false},
    };
    for (int i = 0; i < traffic_styles; ++i) {
        traffic_[static_cast<size_t>(i)] = make_car(traffic[i], 0);
    }
}

} // namespace racer
