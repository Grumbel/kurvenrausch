// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "sprites.hpp"

#include "font.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string_view>
#include <tuple>

namespace racer {

namespace {

constexpr Color Outline{0x14, 0x18, 0x14};

// Is the indicator on `side` (-1 left, +1 right) lit for `signal`: -1 or +1
// that side's, hazard_signal both.
bool lit(int signal, int side) { return signal == side || signal == hazard_signal; }

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

// The back of a billboard: a bare grey panel with bracing on the same posts.
Bitmap make_billboard_back() {
    Bitmap b(96, 64);
    const Color post{0x6a, 0x6a, 0x70}, post_dark{0x44, 0x44, 0x4a};
    const Color panel{0x8c, 0x8c, 0x94}, panel_dark{0x6c, 0x6c, 0x74}, brace{0x52, 0x52, 0x5a};
    paint::rect(b, 14, 40, 6, 24, post);
    paint::rect(b, 14, 40, 2, 24, post_dark);
    paint::rect(b, 76, 40, 6, 24, post);
    paint::rect(b, 76, 40, 2, 24, post_dark);

    paint::rect(b, 0, 0, 96, 42, panel_dark);
    paint::rect(b, 2, 2, 92, 38, panel);
    for (int y : {10, 20, 30}) paint::rect(b, 2, y, 92, 2, brace);
    for (int x : {16, 78}) paint::rect(b, x, 2, 2, 38, brace);
    paint::outline(b, Outline);
    return b;
}

// The gas station: a shop with big windows, and in front of it a canopy on
// two pillars with the FUEL fascia.
Bitmap make_gas_station() {
    Bitmap b(192, 96);
    const Color wall{0xe8, 0xe0, 0xc8}, wall_dark{0xc4, 0xbc, 0xa4}, roof{0x50, 0x50, 0x58};
    const Color glass{0x3c, 0x5c, 0x84}, glass_hi{0x8c, 0xb0, 0xd8}, red{0xc8, 0x20, 0x20};
    const Color white{0xf4, 0xf4, 0xf4}, pillar{0xa8, 0xa8, 0xb0}, pillar_dark{0x80, 0x80, 0x88};

    // Shop.
    paint::rect(b, 76, 36, 114, 60, wall);
    paint::rect(b, 76, 36, 114, 4, roof);
    paint::rect(b, 76, 88, 114, 8, wall_dark);
    for (int x : {86, 124, 160}) {
        paint::rect(b, x, 50, 24, 30, glass);
        paint::stroke(b, static_cast<float>(x) + 4.f, 76.f, static_cast<float>(x) + 14.f, 54.f, 1.5f, 1.5f, glass_hi);
    }
    paint::rect(b, 150, 58, 8, 30, Color{0x6c, 0x48, 0x2c}); // door
    paint::rect(b, 108, 41, 40, 6, red);
    paint::text(b, 115, 41, "SHOP", white);

    // Canopy over the pumps.
    for (int x : {14, 100}) {
        paint::rect(b, x, 28, 7, 68, pillar);
        paint::rect(b, x + 5, 28, 2, 68, pillar_dark);
    }
    paint::rect(b, 0, 8, 122, 22, white);
    paint::rect(b, 0, 12, 122, 14, red);
    paint::rect(b, 0, 28, 122, 2, Color{0xff, 0xf4, 0xc0}); // lights underneath
    const int tx = (122 - font::text_width("FUEL", 2)) / 2;
    paint::text(b, tx, 13, "FUEL", white, 2);

    paint::outline(b, Outline);
    return b;
}

Bitmap make_fuel_pump() {
    Bitmap b(24, 44);
    const Color red{0xc8, 0x20, 0x20}, red_dark{0x90, 0x14, 0x14}, white{0xf4, 0xf4, 0xf4};
    paint::rect(b, 1, 40, 22, 4, Color{0x8c, 0x8c, 0x94}); // island
    paint::rect(b, 3, 6, 18, 34, red);
    paint::rect(b, 17, 6, 4, 34, red_dark);
    paint::rect(b, 3, 1, 18, 6, white);
    paint::rect(b, 6, 11, 12, 7, Color{0x18, 0x20, 0x18}); // display
    paint::rect(b, 7, 13, 10, 1, Color{0x60, 0xf0, 0x80});
    paint::rect(b, 7, 15, 7, 1, Color{0x60, 0xf0, 0x80});
    paint::rect(b, 5, 22, 6, 8, Color{0x30, 0x30, 0x34}); // nozzle holder
    paint::stroke(b, 8.f, 30.f, 2.f, 38.f, 1.2f, 1.2f, Color{0x20, 0x20, 0x24}); // hose
    paint::outline(b, Outline);
    return b;
}

// A tall sign on a pole: FUEL, a pump symbol and the price.
Bitmap make_fuel_sign() {
    Bitmap b(44, 112);
    const Color red{0xc8, 0x20, 0x20}, white{0xf4, 0xf4, 0xf4}, ink{0x20, 0x20, 0x28};
    paint::rect(b, 19, 44, 6, 68, Color{0x9a, 0x9a, 0xa4});
    paint::rect(b, 23, 44, 2, 68, Color{0x6c, 0x6c, 0x74});
    paint::rect(b, 0, 0, 44, 46, red);
    paint::rect(b, 3, 13, 38, 30, white);
    paint::text(b, (44 - font::text_width("FUEL")) / 2, 3, "FUEL", white);
    // Pump symbol.
    paint::rect(b, 15, 16, 10, 14, ink);
    paint::rect(b, 17, 18, 6, 4, white);
    paint::stroke(b, 25.f, 19.f, 29.f, 24.f, 1.f, 1.f, ink);
    paint::stroke(b, 29.f, 24.f, 29.f, 29.f, 1.f, 1.f, ink);
    paint::text(b, (44 - font::text_width("1.89")) / 2, 33, "1.89", ink);
    paint::outline(b, Outline);
    return b;
}

// A San Francisco Victorian: a narrow, tall row house with a pointed gable,
// a bay window, a door up a few steps and white trim.
Bitmap make_victorian(Color body, Color accent) {
    Bitmap b(104, 150);
    const Color trim{0xf4, 0xf0, 0xe4}, shade = blend(body, Color{0x20, 0x20, 0x30}, 0.25f);
    const Color glass{0x30, 0x40, 0x58}, glass_hi{0x80, 0x98, 0xb8}, roof{0x4c, 0x44, 0x48};
    const Color step{0x9c, 0x98, 0x90}, step_dark{0x7c, 0x78, 0x70};

    // Gable roof, then the body.
    for (int y = 4; y < 32; ++y) {
        const int half = (y - 4) * 50 / 28;
        paint::rect(b, 52 - half - 2, y, 2 * half + 4, 1, roof);
        paint::rect(b, 52 - half + 2, y, std::max(0, 2 * half - 4), 1, body);
    }
    paint::rect(b, 51, 0, 2, 5, roof); // finial
    paint::ellipse(b, 52.f, 20.f, 5.f, 5.f, trim); // round attic window
    paint::ellipse(b, 52.f, 20.f, 3.f, 3.f, glass);
    paint::rect(b, 4, 30, 96, 104, body);
    paint::rect(b, 4, 30, 96, 3, trim); // cornice
    for (int x = 6; x < 98; x += 4) paint::rect(b, x, 33, 2, 2, trim); // dentils
    paint::rect(b, 4, 86, 96, 3, trim); // between the floors
    paint::rect(b, 4, 30, 3, 104, trim);
    paint::rect(b, 97, 30, 3, 104, trim);

    // Bay window on the right, two floors high, shaded on its side.
    paint::rect(b, 50, 40, 46, 92, shade);
    paint::rect(b, 54, 40, 42, 92, body);
    for (int y : {46, 94}) {
        for (int x : {57, 72, 85}) {
            paint::rect(b, x - 1, y - 1, 11, 34, trim);
            paint::rect(b, x, y, 9, 32, glass);
            paint::rect(b, x + 1, y + 2, 2, 12, glass_hi);
        }
        paint::rect(b, 54, y + 33, 42, 2, accent); // sills
    }

    // Upstairs window and the door on the left, up the steps.
    paint::rect(b, 13, 44, 26, 36, trim);
    paint::rect(b, 15, 46, 22, 32, glass);
    paint::rect(b, 17, 48, 3, 12, glass_hi);
    paint::rect(b, 11, 40, 30, 4, accent);
    paint::rect(b, 13, 94, 26, 40, trim);
    paint::rect(b, 16, 98, 20, 36, Color{0x6c, 0x3c, 0x24}); // door
    paint::rect(b, 18, 100, 16, 10, glass);
    paint::rect(b, 32, 116, 2, 2, Color{0xe0, 0xc0, 0x60});
    paint::rect(b, 11, 90, 30, 4, accent);
    for (int i = 0; i < 4; ++i) {
        paint::rect(b, 8 - i * 2, 134 + i * 4, 40 + i * 4, 4, i % 2 ? step_dark : step);
    }
    paint::rect(b, 48, 134, 52, 16, Color{0x88, 0x84, 0x7c}); // garage-level wall
    paint::rect(b, 56, 136, 36, 14, Color{0x5c, 0x58, 0x54});
    paint::outline(b, Outline);
    return b;
}

// A motorway-green sign before a fork with a big arrow: dir -1 points to
// the left, +1 to the right.
Bitmap make_fork_sign(int dir) {
    Bitmap b(64, 80);
    const Color green{0x1c, 0x6c, 0x3c}, white{0xf4, 0xf4, 0xf4}, post{0x8c, 0x8c, 0x94};
    paint::rect(b, 12, 40, 5, 40, post);
    paint::rect(b, 47, 40, 5, 40, post);
    paint::rect(b, 0, 0, 64, 46, white);
    paint::rect(b, 2, 2, 60, 42, green);
    // The arrow: a shaft and a head, pointing up and to the side.
    const float d = static_cast<float>(dir);
    paint::stroke(b, 32.f - 10.f * d, 38.f, 32.f + 6.f * d, 16.f, 4.f, 4.f, white);
    for (int i = 0; i < 12; ++i) {
        const int x = 32 + static_cast<int>(10.f * d) - (dir < 0 ? -i : i);
        paint::rect(b, x, 10 + i / 2, 1, 14 - i, white);
    }
    paint::outline(b, Outline);
    return b;
}

// An English hedgerow: a long, low, dense wall of leaves.
Bitmap make_hedge() {
    Bitmap b(96, 40);
    const Color dark{0x1c, 0x4c, 0x1c}, mid{0x2c, 0x6c, 0x28}, light{0x50, 0x94, 0x3c};
    paint::rect(b, 2, 14, 92, 26, mid);
    for (int i = 0; i < 9; ++i) {
        const float x = 6.f + static_cast<float>(i) * 10.5f;
        paint::shaded_ellipse(b, x, 14.f + static_cast<float>((i * 7) % 5), 8.f, 9.f, dark, mid, light);
    }
    for (int x = 2; x < 94; x += 3) paint::rect(b, x, 30 + (x % 4), 2, 10 - (x % 4), dark); // shade at the foot
    paint::outline(b, Outline);
    return b;
}

// A dry-stone wall: uneven courses of grey stones with a row of upright
// coping stones on top.
Bitmap make_stone_wall() {
    Bitmap b(96, 28);
    const Color stones[3] = {{0x8c, 0x88, 0x7c}, {0xa8, 0xa4, 0x96}, {0x74, 0x70, 0x66}};
    const Color gap{0x4c, 0x48, 0x40};
    paint::rect(b, 0, 6, 96, 22, gap);
    for (int row = 0; row < 4; ++row) {
        const int y = 8 + row * 5;
        for (int x = (row % 2) * 4; x < 96; x += 9) {
            paint::rect(b, x, y, 8 - (x + row) % 3, 4, stones[(x / 9 + row) % 3]);
        }
    }
    for (int x = 0; x < 96; x += 4) paint::rect(b, x, 2 + (x % 3), 3, 5 - (x % 3), stones[(x / 4) % 3]);
    paint::outline(b, Outline);
    return b;
}

// A red telephone box with its crown and window panes.
Bitmap make_phone_box() {
    Bitmap b(24, 56);
    const Color red{0xc8, 0x18, 0x18}, red_dark{0x8c, 0x10, 0x10}, glass{0x30, 0x40, 0x50}, white{0xf0, 0xf0, 0xe8};
    paint::rect(b, 2, 6, 20, 50, red);
    paint::rect(b, 4, 2, 16, 5, red);
    paint::rect(b, 10, 0, 4, 3, red_dark);
    paint::rect(b, 4, 8, 16, 3, Color{0x18, 0x18, 0x18});
    paint::rect(b, 6, 9, 12, 1, white);
    for (int y = 14; y < 44; y += 6)
        for (int x = 5; x < 19; x += 5) paint::rect(b, x, y, 4, 5, glass);
    paint::rect(b, 18, 6, 4, 50, red_dark);
    paint::outline(b, Outline);
    return b;
}

// A Dutch windmill: a tapering tower with a cap and four lattice sails.
Bitmap make_windmill() {
    Bitmap b(96, 150);
    const Color tower{0x6c, 0x4c, 0x34}, tower_light{0x8c, 0x68, 0x48}, cap{0x3c, 0x34, 0x30};
    const Color sail{0xe8, 0xe0, 0xd0}, frame{0x50, 0x3c, 0x2c}, door{0x2c, 0x5c, 0x3c};
    for (int y = 60; y < 150; ++y) {
        const int half = 10 + (y - 60) * 12 / 90;
        paint::rect(b, 48 - half, y, 2 * half, 1, tower);
        paint::rect(b, 48 - half, y, half / 2, 1, tower_light);
    }
    paint::rect(b, 30, 112, 36, 3, frame); // the stage around it
    paint::rect(b, 43, 128, 10, 22, door);
    paint::rect(b, 44, 84, 8, 10, Color{0xe8, 0xe0, 0xd0});
    paint::shaded_ellipse(b, 48.f, 58.f, 13.f, 9.f, cap, cap, Color{0x5c, 0x50, 0x48});
    // Four sails, at an angle so the mill looks like it turns.
    const float cx = 48.f, cy = 58.f;
    for (int k = 0; k < 4; ++k) {
        const float a = 0.5f + static_cast<float>(k) * 1.5707963f;
        const float ux = std::cos(a), uy = std::sin(a), vx = -uy, vy = ux;
        for (float t = 8.f; t < 46.f; t += 1.f) {
            for (float w = 0.f; w < 9.f; w += 1.f) {
                const bool lattice = static_cast<int>(t) % 6 == 0 || w < 1.f || w > 7.5f;
                b.set(static_cast<int>(cx + ux * t + vx * w), static_cast<int>(cy + uy * t + vy * w), lattice ? frame : sail);
            }
        }
        paint::stroke(b, cx, cy, cx + ux * 46.f, cy + uy * 46.f, 2.f, 1.f, frame);
    }
    paint::outline(b, Outline);
    return b;
}

// A strip of tulip field: rows of red, yellow and pink blooms over green.
Bitmap make_tulips() {
    Bitmap b(96, 20);
    const Color leaf{0x3c, 0x88, 0x2c}, leaf_dark{0x2c, 0x6c, 0x24};
    const Color blooms[3] = {{0xe0, 0x20, 0x30}, {0xf8, 0xd0, 0x20}, {0xf0, 0x80, 0xb0}};
    paint::rect(b, 0, 8, 96, 12, leaf_dark);
    for (int band = 0; band < 3; ++band) {
        const int x0 = band * 32;
        for (int x = x0; x < x0 + 32; x += 2) {
            paint::rect(b, x, 9 + (x % 3), 1, 11 - (x % 3), leaf);
            paint::rect(b, x, 5 + ((x * 7) % 4), 2, 3, blooms[band]);
        }
    }
    paint::outline(b, Outline);
    return b;
}

// A cherry tree in blossom: a dark, crooked trunk under clouds of pink.
Bitmap make_cherry_tree() {
    Bitmap b(80, 96);
    const Color bark{0x4c, 0x30, 0x2c}, bark_dark{0x30, 0x1c, 0x1c};
    const Color dark{0xc8, 0x70, 0x94}, mid{0xf0, 0xa4, 0xc0}, light{0xff, 0xdc, 0xe8};
    paint::stroke(b, 40.f, 95.f, 38.f, 50.f, 8.f, 5.f, bark);
    paint::stroke(b, 39.f, 70.f, 22.f, 46.f, 3.f, 2.f, bark_dark);
    paint::stroke(b, 39.f, 64.f, 58.f, 44.f, 3.f, 2.f, bark_dark);
    paint::shaded_ellipse(b, 40.f, 36.f, 30.f, 20.f, dark, mid, light);
    paint::shaded_ellipse(b, 18.f, 46.f, 15.f, 11.f, dark, mid, light);
    paint::shaded_ellipse(b, 62.f, 44.f, 15.f, 11.f, dark, mid, light);
    paint::shaded_ellipse(b, 30.f, 22.f, 15.f, 12.f, dark, mid, light);
    paint::shaded_ellipse(b, 52.f, 22.f, 15.f, 12.f, dark, mid, light);
    for (int i = 0; i < 18; ++i) b.set(8 + (i * 37) % 64, 60 + (i * 13) % 34, light); // falling petals
    paint::outline(b, Outline);
    return b;
}

// A red torii, the gate to a shrine.
Bitmap make_torii() {
    Bitmap b(80, 80);
    const Color red{0xd0, 0x30, 0x20}, red_dark{0x98, 0x20, 0x14}, black{0x20, 0x1c, 0x1c};
    paint::rect(b, 14, 16, 7, 64, red);
    paint::rect(b, 59, 16, 7, 64, red);
    paint::rect(b, 19, 16, 2, 64, red_dark);
    paint::rect(b, 64, 16, 2, 64, red_dark);
    paint::rect(b, 0, 6, 80, 5, black); // the top beam, curving up at the ends
    paint::rect(b, 0, 4, 6, 3, black);
    paint::rect(b, 74, 4, 6, 3, black);
    paint::rect(b, 3, 11, 74, 4, red);
    paint::rect(b, 8, 24, 64, 5, red);
    paint::rect(b, 37, 15, 6, 9, red); // the tablet between the beams
    paint::rect(b, 14, 74, 7, 6, black);
    paint::rect(b, 59, 74, 7, 6, black);
    paint::outline(b, Outline);
    return b;
}

// A stone lantern.
Bitmap make_stone_lantern() {
    Bitmap b(16, 32);
    const Color stone{0x9c, 0x98, 0x8c}, shade{0x74, 0x70, 0x66}, light{0xff, 0xe8, 0xa0};
    paint::rect(b, 3, 28, 10, 4, stone);
    paint::rect(b, 6, 16, 4, 12, stone);
    paint::rect(b, 3, 12, 10, 5, stone);
    paint::rect(b, 5, 13, 6, 3, light);
    paint::rect(b, 1, 8, 14, 4, shade);
    paint::rect(b, 4, 5, 8, 3, stone);
    paint::rect(b, 7, 2, 2, 3, stone);
    paint::outline(b, Outline);
    return b;
}

// A small car as seen through a showroom window, for the dealer's sprites.
void showroom_car(Bitmap& b, int x, int y, Color body, Color light) {
    paint::rect(b, x + 4, y, 16, 5, body);
    paint::rect(b, x + 7, y + 1, 10, 3, Color{0x30, 0x40, 0x58});
    paint::rect(b, x, y + 5, 24, 7, body);
    paint::rect(b, x + 1, y + 5, 22, 1, light);
    paint::rect(b, x + 2, y + 12, 5, 3, Color{0x18, 0x18, 0x1c});
    paint::rect(b, x + 17, y + 12, 5, 3, Color{0x18, 0x18, 0x1c});
}

// A car dealer's showroom: a glass front with cars on display, the CARS
// sign on the roof and strings of pennants.
// A dealer's showroom: four cars (body and highlight colours) behind the
// glass, the dealer's name in its colour on the roof.
struct ShowroomLook {
    const char* name;
    Color color, light;
    Color cars[4][2];
};

Bitmap make_showroom(const ShowroomLook& look) {
    Bitmap b(192, 96);
    const Color wall{0xe4, 0xe8, 0xec}, frame{0x8c, 0x94, 0x9c}, glass{0x9c, 0xc0, 0xdc}, floor{0xc8, 0xcc, 0xd0};
    const Color blue = look.color, white{0xf4, 0xf4, 0xf4};
    paint::rect(b, 4, 30, 184, 66, wall);
    paint::rect(b, 12, 40, 168, 50, glass);
    paint::rect(b, 12, 80, 168, 10, floor);
    for (int x = 12; x <= 180; x += 42) paint::rect(b, x, 40, 3, 50, frame);
    for (int i = 0; i < 4; ++i) showroom_car(b, 22 + 42 * i, 68, look.cars[i][0], look.cars[i][1]);
    for (int x = 20; x < 180; x += 38) paint::stroke(b, static_cast<float>(x), 44.f, static_cast<float>(x) + 10.f, 60.f, 1.f, 1.f, white);
    paint::rect(b, 4, 24, 184, 7, blue);
    paint::rect(b, 56, 4, 80, 22, blue);
    paint::rect(b, 58, 6, 76, 18, look.light);
    paint::text(b, (192 - font::text_width(look.name, 2)) / 2, 8, look.name, white, 2);
    // Pennants strung from the roof.
    const Color pennant[3] = {{0xe0, 0x20, 0x30}, {0xf8, 0xd0, 0x20}, {0x30, 0x70, 0xe0}};
    for (int i = 0; i < 23; ++i) {
        const int x = 6 + i * 8, y = 31 + (i % 2);
        for (int k = 0; k < 4; ++k) paint::rect(b, x + k, y, 1, 6 - k, pennant[i % 3]);
    }
    paint::outline(b, Outline);
    return b;
}

// The dealer's tall sign: its name and a car.
Bitmap make_dealer_sign(const ShowroomLook& look) {
    Bitmap b(44, 112);
    const Color blue = look.color, white{0xf4, 0xf4, 0xf4};
    paint::rect(b, 19, 44, 6, 68, Color{0x9a, 0x9a, 0xa4});
    paint::rect(b, 23, 44, 2, 68, Color{0x6c, 0x6c, 0x74});
    paint::rect(b, 0, 0, 44, 46, blue);
    paint::rect(b, 3, 13, 38, 30, white);
    paint::text(b, (44 - font::text_width(look.name)) / 2, 3, look.name, white);
    showroom_car(b, 10, 20, look.cars[0][0], look.cars[0][1]);
    paint::outline(b, Outline);
    return b;
}

// A car wash: a wash bay open to the road, its two big brushes inside, a
// soap-bubble sign on the roof.
Bitmap make_car_wash() {
    Bitmap b(192, 96);
    const Color wall{0xe8, 0xf0, 0xf4}, trim{0x20, 0x90, 0xc8}, inside{0x30, 0x3c, 0x48}, floor{0x58, 0x64, 0x6c};
    const Color white{0xf4, 0xf4, 0xf4};
    paint::rect(b, 4, 30, 184, 66, wall);
    paint::rect(b, 4, 24, 184, 7, trim);
    paint::rect(b, 24, 40, 144, 56, inside);
    paint::rect(b, 24, 88, 144, 8, floor);
    paint::rect(b, 24, 40, 144, 3, Color{0x9c, 0xa8, 0xb4}); // the gantry rail
    // The brushes: tall cylinders of blue and red bristles.
    const Color bristle[2][3] = {{{0x18, 0x48, 0xa0}, {0x30, 0x70, 0xd0}, {0x80, 0xb0, 0xf0}},
                                 {{0x98, 0x18, 0x28}, {0xd0, 0x30, 0x40}, {0xf0, 0x80, 0x88}}};
    for (int k = 0; k < 2; ++k) {
        const int x = k ? 120 : 48;
        paint::rect(b, x + 11, 43, 2, 6, Color{0x9c, 0xa8, 0xb4});
        for (int y = 49; y < 92; ++y) {
            paint::rect(b, x, y, 24, 1, bristle[k][1]);
            paint::rect(b, x, y, 4, 1, bristle[k][0]);
            paint::rect(b, x + 20, y, 4, 1, bristle[k][0]);
            if (y % 4 == 0) paint::rect(b, x + 6, y, 8, 1, bristle[k][2]);
        }
    }
    // Water dripping in the bay.
    for (int x = 30; x < 166; x += 9) paint::rect(b, x, 46 + (x * 7) % 30, 1, 3, Color{0xa0, 0xd0, 0xf0});
    // The sign: WASH among soap bubbles.
    paint::rect(b, 52, 2, 88, 24, trim);
    paint::rect(b, 54, 4, 84, 20, Color{0x40, 0xb0, 0xe0});
    paint::text(b, (192 - font::text_width("WASH", 2)) / 2, 8, "WASH", white, 2);
    for (const auto& [x, y, r] : {std::tuple{50.f, 6.f, 4.f}, {144.f, 10.f, 5.f}, {138.f, 2.f, 3.f}, {46.f, 18.f, 3.f}}) {
        paint::shaded_ellipse(b, x, y, r, r, Color{0x90, 0xc8, 0xe8}, Color{0xd8, 0xf0, 0xff}, white);
    }
    paint::outline(b, Outline);
    return b;
}

// The car wash's tall sign: WASH over a car in the bubbles.
Bitmap make_wash_sign() {
    Bitmap b(44, 112);
    const Color trim{0x20, 0x90, 0xc8}, white{0xf4, 0xf4, 0xf4};
    paint::rect(b, 19, 44, 6, 68, Color{0x9a, 0x9a, 0xa4});
    paint::rect(b, 23, 44, 2, 68, Color{0x6c, 0x6c, 0x74});
    paint::rect(b, 0, 0, 44, 46, trim);
    paint::rect(b, 3, 13, 38, 30, Color{0xd8, 0xf0, 0xff});
    paint::text(b, (44 - font::text_width("WASH")) / 2, 3, "WASH", white);
    showroom_car(b, 10, 24, Color{0xd0, 0x18, 0x1c}, Color{0xf0, 0x60, 0x50});
    for (const auto& [x, y, r] : {std::tuple{9.f, 19.f, 3.f}, {33.f, 18.f, 4.f}, {22.f, 17.f, 2.f}}) {
        paint::shaded_ellipse(b, x, y, r, r, Color{0x90, 0xc8, 0xe8}, white, white);
    }
    paint::outline(b, Outline);
    return b;
}

// A roadside motel: a long row of rooms, doors in turn with windows, under a
// flat roof.
Bitmap make_motel() {
    Bitmap b(192, 96);
    const Color wall{0xf0, 0xe0, 0xc4}, roof{0x2c, 0x9c, 0x98}, roof_dark{0x1c, 0x6c, 0x68};
    const Color door{0xd0, 0x50, 0x70}, glass{0x60, 0x90, 0xb0}, curtain{0xf0, 0xc8, 0x60};
    paint::rect(b, 6, 44, 180, 52, wall);
    paint::rect(b, 0, 36, 192, 8, roof);
    paint::rect(b, 0, 43, 192, 2, roof_dark);
    for (int i = 0; i < 8; ++i) {
        const int x = 12 + i * 22;
        paint::rect(b, x, 60, 8, 34, door);
        paint::rect(b, x + 6, 76, 1, 2, Color{0xf0, 0xd0, 0x40}); // knob
        paint::rect(b, x + 1, 52, 6, 5, Color{0xf8, 0xf8, 0xf0});  // room number
        paint::rect(b, x + 3, 53, 2, 3, Color{0x30, 0x30, 0x30});
        paint::rect(b, x + 10, 60, 10, 12, glass);
        paint::rect(b, x + 10, 60, 3, 12, curtain);
        paint::rect(b, x + 17, 60, 3, 12, curtain);
    }
    paint::rect(b, 0, 94, 192, 2, Color{0x90, 0x88, 0x78}); // the walkway
    // The office at the end, with its sign on the roof.
    paint::rect(b, 56, 14, 80, 22, roof_dark);
    paint::rect(b, 58, 16, 76, 18, Color{0x18, 0x18, 0x30});
    paint::text(b, (192 - font::text_width("MOTEL", 2)) / 2, 18, "MOTEL", Color{0xff, 0x50, 0x90}, 2);
    paint::outline(b, Outline);
    return b;
}

// The motel's neon sign: MOTEL down a red panel, VACANCY glowing below.
Bitmap make_motel_sign() {
    Bitmap b(44, 112);
    paint::rect(b, 19, 60, 6, 52, Color{0x9a, 0x9a, 0xa4});
    paint::rect(b, 23, 60, 2, 52, Color{0x6c, 0x6c, 0x74});
    paint::rect(b, 8, 0, 28, 50, Color{0xc0, 0x18, 0x30});
    paint::rect(b, 10, 2, 24, 46, Color{0x80, 0x10, 0x20});
    const char* letters = "MOTEL";
    for (int i = 0; i < 5; ++i) {
        const char s[2] = {letters[i], 0};
        paint::text(b, 20, 4 + i * 9, s, Color{0xff, 0xe0, 0x90});
    }
    paint::rect(b, 0, 50, 44, 11, Color{0x18, 0x30, 0x20});
    paint::text(b, 1, 52, "VACANCY", Color{0x60, 0xff, 0x80});
    paint::outline(b, Outline);
    return b;
}

// A hospital: white, rows of windows, a red cross on the roof and the
// emergency entrance at the front.
Bitmap make_hospital() {
    Bitmap b(192, 96);
    const Color wall{0xf4, 0xf4, 0xf0}, shade{0xc8, 0xcc, 0xd0}, glass{0x80, 0xb0, 0xd8}, red{0xd8, 0x20, 0x20};
    paint::rect(b, 4, 20, 184, 76, wall);
    paint::rect(b, 4, 20, 184, 3, shade);
    for (int row = 0; row < 3; ++row) {
        for (int x = 12; x < 180; x += 14) paint::rect(b, x, 28 + row * 14, 9, 8, glass);
    }
    paint::rect(b, 0, 70, 192, 4, Color{0x2c, 0x5c, 0xb0}); // a blue band
    paint::rect(b, 70, 74, 52, 22, Color{0x50, 0x70, 0x88});  // the entrance
    paint::rect(b, 72, 76, 48, 20, glass);
    paint::rect(b, 95, 76, 2, 20, Color{0x50, 0x70, 0x88});
    paint::rect(b, 64, 64, 64, 8, red);
    paint::text(b, (192 - font::text_width("HOSPITAL")) / 2, 64, "HOSPITAL", Color{0xff, 0xff, 0xff});
    // The cross on the roof.
    paint::rect(b, 86, 0, 20, 20, wall);
    paint::rect(b, 93, 3, 6, 14, red);
    paint::rect(b, 89, 7, 14, 6, red);
    paint::outline(b, Outline);
    return b;
}

// The blue road sign with a white H.
Bitmap make_hospital_sign() {
    Bitmap b(44, 112);
    paint::rect(b, 19, 40, 6, 72, Color{0x9a, 0x9a, 0xa4});
    paint::rect(b, 23, 40, 2, 72, Color{0x6c, 0x6c, 0x74});
    paint::rect(b, 2, 0, 40, 40, Color{0xf4, 0xf4, 0xf4});
    paint::rect(b, 4, 2, 36, 36, Color{0x1c, 0x4c, 0xa8});
    paint::text(b, (44 - font::text_width("H", 4)) / 2, 6, "H", Color{0xf4, 0xf4, 0xf4}, 4);
    paint::outline(b, Outline);
    return b;
}

// A truck stop: a diner with a long row of windows under a big sign.
Bitmap make_truckstop() {
    Bitmap b(192, 96);
    const Color wall{0xd8, 0xcc, 0xb0}, trim{0xb0, 0x20, 0x20}, glass{0x70, 0xa0, 0xc0}, yellow{0xf8, 0xc8, 0x20};
    paint::rect(b, 4, 36, 184, 60, wall);
    paint::rect(b, 0, 32, 192, 6, trim);
    for (int x = 12; x < 176; x += 20) paint::rect(b, x, 46, 16, 18, glass);
    paint::rect(b, 0, 66, 192, 4, trim);
    paint::rect(b, 86, 72, 20, 24, Color{0x60, 0x40, 0x28}); // the door
    paint::rect(b, 88, 74, 16, 10, glass);
    paint::text(b, 22, 78, "DINER", trim);
    paint::text(b, 130, 78, "DIESEL", trim);
    // The sign on the roof, on two legs.
    paint::rect(b, 50, 22, 4, 10, Color{0x6c, 0x6c, 0x74});
    paint::rect(b, 138, 22, 4, 10, Color{0x6c, 0x6c, 0x74});
    paint::rect(b, 30, 0, 132, 24, trim);
    paint::rect(b, 32, 2, 128, 20, yellow);
    paint::text(b, (192 - font::text_width("TRUCK STOP", 2)) / 2, 5, "TRUCK STOP", Color{0x20, 0x18, 0x10}, 2);
    paint::outline(b, Outline);
    return b;
}

// The truck stop's tall sign: TRUCK STOP in black on yellow.
Bitmap make_truck_sign() {
    Bitmap b(44, 112);
    paint::rect(b, 19, 40, 6, 72, Color{0x9a, 0x9a, 0xa4});
    paint::rect(b, 23, 40, 2, 72, Color{0x6c, 0x6c, 0x74});
    paint::rect(b, 0, 0, 44, 40, Color{0xb0, 0x20, 0x20});
    paint::rect(b, 2, 2, 40, 36, Color{0xf8, 0xc8, 0x20});
    paint::text(b, (44 - font::text_width("TRUCK")) / 2, 8, "TRUCK", Color{0x20, 0x18, 0x10});
    paint::text(b, (44 - font::text_width("STOP")) / 2, 22, "STOP", Color{0x20, 0x18, 0x10});
    paint::outline(b, Outline);
    return b;
}

// A date palm: a tall, ringed trunk under a crown of drooping fronds.
Bitmap make_date_palm() {
    Bitmap b(64, 112);
    const Color trunk{0x8c, 0x6c, 0x44}, ring{0x6c, 0x50, 0x30};
    const Color dark{0x2c, 0x5c, 0x24}, mid{0x44, 0x80, 0x30}, light{0x6c, 0xa4, 0x40};
    for (int y = 30; y < 112; ++y) {
        const int x = 32 + static_cast<int>(std::sin(static_cast<float>(y) * 0.03f) * 3.f);
        paint::rect(b, x - 3, y, 7, 1, y % 5 ? trunk : ring);
    }
    for (int k = 0; k < 9; ++k) {
        const float a = -2.8f + static_cast<float>(k) * 0.7f;
        paint::stroke(b, 32.f, 28.f, 32.f + std::cos(a) * 30.f, 28.f + std::sin(a) * 14.f + 14.f, 4.f, 1.f,
                      k % 3 == 0 ? light : k % 3 == 1 ? mid : dark);
    }
    paint::shaded_ellipse(b, 32.f, 28.f, 6.f, 5.f, dark, mid, light);
    paint::rect(b, 27, 32, 3, 4, Color{0xa0, 0x58, 0x28}); // dates
    paint::rect(b, 34, 32, 3, 4, Color{0xa0, 0x58, 0x28});
    paint::outline(b, Outline);
    return b;
}

// A pyramid in the distance: a lit face and a shaded one, in courses.
Bitmap make_pyramid() {
    Bitmap b(160, 80);
    const Color lit{0xe4, 0xc8, 0x88}, shade{0xb0, 0x90, 0x5c}, course{0xc8, 0xa8, 0x70};
    for (int y = 0; y < 80; ++y) {
        const int half = y;
        paint::rect(b, 80 - half, y, half, 1, lit);
        paint::rect(b, 80, y, half, 1, shade);
        if (y % 6 == 5) paint::rect(b, 80 - half, y, half, 1, course);
    }
    paint::outline(b, Outline);
    return b;
}

// An acacia: a thin, forked trunk under a wide, flat crown.
Bitmap make_acacia() {
    Bitmap b(96, 64);
    const Color bark{0x5c, 0x40, 0x2c}, dark{0x3c, 0x5c, 0x24}, mid{0x58, 0x7c, 0x30}, light{0x7c, 0x9c, 0x40};
    paint::stroke(b, 48.f, 63.f, 46.f, 30.f, 4.f, 3.f, bark);
    paint::stroke(b, 46.f, 34.f, 30.f, 20.f, 2.f, 1.5f, bark);
    paint::stroke(b, 46.f, 32.f, 64.f, 18.f, 2.f, 1.5f, bark);
    for (int k = 0; k < 6; ++k) {
        paint::shaded_ellipse(b, 14.f + static_cast<float>(k) * 13.6f, 14.f + static_cast<float>(k % 2) * 2.f, 12.f, 6.f,
                              dark, mid, light);
    }
    paint::outline(b, Outline);
    return b;
}

// A giraffe, side on, with its long neck and patches.
Bitmap make_giraffe() {
    Bitmap b(40, 96);
    const Color coat{0xe0, 0xb4, 0x5c}, patch{0x8c, 0x58, 0x2c}, dark{0x5c, 0x3c, 0x20};
    for (int x : {10, 14, 26, 30}) paint::rect(b, x, 62, 3, 34, coat);
    paint::shaded_ellipse(b, 20.f, 58.f, 14.f, 9.f, patch, coat, coat);
    paint::stroke(b, 28.f, 52.f, 32.f, 12.f, 6.f, 4.f, coat);
    paint::shaded_ellipse(b, 33.f, 9.f, 5.f, 4.f, coat, coat, coat);
    paint::rect(b, 31, 3, 1, 3, dark);
    paint::rect(b, 34, 3, 1, 3, dark);
    for (int i = 0; i < 12; ++i) paint::rect(b, 10 + (i * 7) % 20, 52 + (i * 5) % 12, 3, 2, patch);
    for (int y = 18; y < 50; y += 6) paint::rect(b, 29 + (y / 6) % 3, y, 2, 2, patch);
    paint::rect(b, 6, 56, 2, 12, dark); // the tail
    paint::outline(b, Outline);
    return b;
}

// A termite mound: a tall, knobbly spire of red earth.
Bitmap make_termite_mound() {
    Bitmap b(24, 48);
    const Color dark{0x8c, 0x44, 0x24}, mid{0xb4, 0x5c, 0x30}, light{0xd0, 0x7c, 0x48};
    paint::shaded_ellipse(b, 12.f, 40.f, 11.f, 8.f, dark, mid, light);
    paint::shaded_ellipse(b, 11.f, 26.f, 7.f, 14.f, dark, mid, light);
    paint::shaded_ellipse(b, 13.f, 9.f, 4.f, 8.f, dark, mid, light);
    paint::outline(b, Outline);
    return b;
}

// A banyan: a huge spreading crown with aerial roots hanging down.
Bitmap make_banyan() {
    Bitmap b(96, 80);
    const Color bark{0x6c, 0x58, 0x44}, dark{0x24, 0x54, 0x24}, mid{0x34, 0x78, 0x30}, light{0x58, 0x9c, 0x40};
    paint::stroke(b, 48.f, 79.f, 48.f, 36.f, 12.f, 9.f, bark);
    for (int x = 12; x < 88; x += 7) paint::rect(b, x, 30 + (x % 5), 1, 50 - 30 - (x % 5) + 20, bark);
    paint::shaded_ellipse(b, 48.f, 24.f, 46.f, 20.f, dark, mid, light);
    paint::shaded_ellipse(b, 24.f, 30.f, 20.f, 12.f, dark, mid, light);
    paint::shaded_ellipse(b, 72.f, 30.f, 20.f, 12.f, dark, mid, light);
    paint::outline(b, Outline);
    return b;
}

// A Hindu temple: a stepped base, a doorway and a tall curved tower with a
// saffron flag.
Bitmap make_temple() {
    Bitmap b(80, 112);
    const Color stone{0xe0, 0xc8, 0x98}, shade{0xb8, 0x9c, 0x70}, door{0x4c, 0x30, 0x24}, saffron{0xf0, 0x80, 0x20};
    paint::rect(b, 4, 96, 72, 16, shade);
    paint::rect(b, 10, 72, 60, 24, stone);
    paint::rect(b, 34, 80, 12, 16, door);
    for (int y = 10; y < 72; ++y) {
        const float t = static_cast<float>(y - 10) / 62.f;
        const int half = 6 + static_cast<int>(22.f * std::sqrt(t));
        paint::rect(b, 40 - half, y, 2 * half, 1, y % 6 < 2 ? shade : stone);
    }
    paint::shaded_ellipse(b, 40.f, 9.f, 6.f, 4.f, shade, stone, stone);
    paint::rect(b, 39, 0, 2, 6, door);
    paint::rect(b, 41, 0, 8, 4, saffron);
    paint::outline(b, Outline);
    return b;
}

// A white zebu cow with its hump, standing by the road.
Bitmap make_cow() {
    Bitmap b(40, 28);
    const Color white{0xec, 0xe8, 0xe0}, shade{0xc0, 0xb8, 0xac}, dark{0x4c, 0x40, 0x38};
    for (int x : {8, 12, 26, 30}) paint::rect(b, x, 18, 3, 10, shade);
    paint::shaded_ellipse(b, 20.f, 14.f, 14.f, 7.f, shade, white, white);
    paint::shaded_ellipse(b, 17.f, 8.f, 4.f, 3.f, shade, white, white); // hump
    paint::shaded_ellipse(b, 34.f, 12.f, 4.f, 4.f, shade, white, white);
    paint::rect(b, 32, 6, 1, 3, dark);
    paint::rect(b, 36, 6, 1, 3, dark);
    paint::rect(b, 5, 12, 1, 9, dark);
    paint::outline(b, Outline);
    return b;
}

// A maple in autumn: red, orange and gold.
Bitmap make_maple() {
    Bitmap b(64, 80);
    const Color bark{0x4c, 0x34, 0x28}, dark{0x9c, 0x20, 0x18}, mid{0xd8, 0x44, 0x20}, light{0xf8, 0x9c, 0x30};
    paint::stroke(b, 32.f, 79.f, 32.f, 40.f, 7.f, 4.f, bark);
    paint::shaded_ellipse(b, 32.f, 30.f, 26.f, 20.f, dark, mid, light);
    paint::shaded_ellipse(b, 18.f, 40.f, 12.f, 9.f, dark, mid, light);
    paint::shaded_ellipse(b, 46.f, 40.f, 12.f, 9.f, dark, mid, light);
    paint::shaded_ellipse(b, 32.f, 14.f, 14.f, 10.f, dark, mid, light);
    for (int i = 0; i < 10; ++i) b.set(6 + (i * 23) % 52, 66 + (i * 7) % 13, light); // fallen leaves
    paint::outline(b, Outline);
    return b;
}

// A Korean hanok gate: red pillars under a dark tiled roof whose ends curve up.
Bitmap make_hanok_gate() {
    Bitmap b(96, 72);
    const Color red{0xa8, 0x30, 0x24}, green{0x2c, 0x6c, 0x5c}, roof{0x3c, 0x40, 0x48}, tile{0x58, 0x5c, 0x64};
    paint::rect(b, 20, 30, 7, 42, red);
    paint::rect(b, 69, 30, 7, 42, red);
    paint::rect(b, 20, 26, 56, 6, green); // painted beams
    paint::rect(b, 24, 28, 48, 1, Color{0xe0, 0xc0, 0x40});
    for (int y = 6; y < 26; ++y) {
        const float t = static_cast<float>(y - 6) / 20.f;
        const int half = 30 + static_cast<int>(18.f * t);
        paint::rect(b, 48 - half, y, 2 * half, 1, y % 4 == 0 ? tile : roof);
    }
    paint::stroke(b, 2.f, 18.f, 0.f, 12.f, 3.f, 2.f, roof); // the upturned ends
    paint::stroke(b, 94.f, 18.f, 96.f, 12.f, 3.f, 2.f, roof);
    paint::rect(b, 34, 2, 28, 5, roof);
    paint::outline(b, Outline);
    return b;
}

// A gum tree: a pale, smooth trunk and sparse grey-green foliage.
Bitmap make_gum_tree() {
    Bitmap b(56, 96);
    const Color bark{0xe4, 0xdc, 0xd0}, bark_dark{0xb0, 0xa4, 0x94};
    const Color dark{0x4c, 0x60, 0x48}, mid{0x6c, 0x84, 0x64}, light{0x94, 0xa8, 0x84};
    paint::stroke(b, 28.f, 95.f, 26.f, 30.f, 6.f, 3.f, bark);
    paint::stroke(b, 29.f, 95.f, 28.f, 30.f, 2.f, 1.f, bark_dark);
    paint::stroke(b, 26.f, 50.f, 12.f, 30.f, 2.f, 1.f, bark);
    paint::stroke(b, 27.f, 44.f, 42.f, 26.f, 2.f, 1.f, bark);
    paint::shaded_ellipse(b, 14.f, 24.f, 12.f, 9.f, dark, mid, light);
    paint::shaded_ellipse(b, 40.f, 20.f, 13.f, 10.f, dark, mid, light);
    paint::shaded_ellipse(b, 26.f, 12.f, 12.f, 9.f, dark, mid, light);
    paint::outline(b, Outline);
    return b;
}

// The yellow diamond warning of kangaroos.
Bitmap make_kangaroo_sign() {
    Bitmap b(32, 56);
    const Color yellow{0xf8, 0xd0, 0x20}, black{0x18, 0x18, 0x18};
    paint::rect(b, 15, 28, 3, 28, Color{0x9a, 0x9a, 0xa4});
    for (int y = 0; y < 30; ++y) {
        const int half = y < 15 ? y : 29 - y;
        paint::rect(b, 16 - half, y, 2 * half + 1, 1, black);
        if (half > 1) paint::rect(b, 17 - half, y, 2 * half - 1, 1, yellow);
    }
    // The kangaroo: body, tail, head and ears.
    paint::shaded_ellipse(b, 15.f, 16.f, 4.f, 5.f, black, black, black);
    paint::stroke(b, 12.f, 19.f, 6.f, 22.f, 1.5f, 1.f, black);
    paint::shaded_ellipse(b, 19.f, 9.f, 2.f, 2.f, black, black, black);
    paint::rect(b, 19, 6, 1, 2, black);
    paint::stroke(b, 15.f, 21.f, 19.f, 22.f, 1.f, 1.f, black);
    paint::outline(b, Outline);
    return b;
}

// Uluru: a long, flat-topped red rock with steep ends and grooves down its
// flanks.
Bitmap make_uluru() {
    Bitmap b(192, 56);
    const Color dark{0x8c, 0x30, 0x18}, mid{0xc0, 0x4c, 0x24}, light{0xe0, 0x74, 0x3c};
    for (int x = 0; x < 192; ++x) {
        const float t = (static_cast<float>(x) - 96.f) / 96.f;
        const float t2 = t * t;
        const int top = 8 + static_cast<int>(48.f * t2 * t2 * t2 + 4.f * t2); // flat-topped, steep at the ends
        if (top >= 56) continue;
        for (int y = top; y < 56; ++y) {
            Color c = x < 70 ? light : x < 130 ? mid : dark;
            if (x % 13 == 0 && y > top + 4) c = dark; // grooves
            b.set(x, y, c);
        }
    }
    paint::outline(b, Outline);
    return b;
}

// A rainforest giant: a tall trunk with vines under layered dense crowns.
Bitmap make_jungle_tree() {
    Bitmap b(80, 112);
    const Color bark{0x4c, 0x40, 0x30}, vine{0x2c, 0x60, 0x24};
    const Color dark{0x14, 0x40, 0x18}, mid{0x24, 0x60, 0x24}, light{0x3c, 0x84, 0x30};
    paint::stroke(b, 40.f, 111.f, 40.f, 30.f, 8.f, 6.f, bark);
    for (int x : {36, 43}) paint::stroke(b, static_cast<float>(x), 40.f, static_cast<float>(x) + 2.f, 100.f, 1.f, 1.f, vine);
    paint::shaded_ellipse(b, 40.f, 34.f, 38.f, 16.f, dark, mid, light);
    paint::shaded_ellipse(b, 26.f, 22.f, 20.f, 13.f, dark, mid, light);
    paint::shaded_ellipse(b, 54.f, 22.f, 20.f, 13.f, dark, mid, light);
    paint::shaded_ellipse(b, 40.f, 12.f, 18.f, 11.f, dark, mid, light);
    paint::outline(b, Outline);
    return b;
}

// A banana plant: big, broad, ragged leaves on a short stem.
Bitmap make_banana() {
    Bitmap b(40, 56);
    const Color stem{0x6c, 0x80, 0x3c}, dark{0x2c, 0x70, 0x24}, light{0x5c, 0xa8, 0x38};
    paint::stroke(b, 20.f, 55.f, 20.f, 28.f, 4.f, 3.f, stem);
    for (int k = 0; k < 6; ++k) {
        const float a = -2.9f + static_cast<float>(k) * 0.55f;
        paint::stroke(b, 20.f, 28.f, 20.f + std::cos(a) * 19.f, 26.f + std::sin(a) * 22.f, 5.f, 2.f, k % 2 ? light : dark);
    }
    paint::outline(b, Outline);
    return b;
}

// A street lamp: a dark green pole with a lantern.
Bitmap make_street_lamp() {
    Bitmap b(24, 112);
    const Color pole{0x24, 0x3c, 0x2c}, pole_hi{0x3c, 0x5c, 0x44};
    paint::rect(b, 7, 104, 10, 8, pole);
    paint::rect(b, 10, 22, 4, 84, pole);
    paint::rect(b, 10, 22, 1, 84, pole_hi);
    paint::rect(b, 5, 8, 14, 14, pole);
    paint::rect(b, 7, 10, 10, 10, Color{0xff, 0xec, 0xb0});
    paint::rect(b, 8, 11, 4, 4, Color{0xff, 0xff, 0xf0});
    paint::rect(b, 3, 6, 18, 3, pole);
    paint::rect(b, 11, 2, 2, 4, pole);
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

namespace {
void draw_head(Bitmap& b, float x, float y, float r, const Person& p);
void heads_behind_glass(Bitmap& b, float u, int top, const Person& driver, const Person& passenger,
                        bool bandaged = false);
} // namespace

Bitmap make_car(const CarStyle& style, int turn, int signal, bool brake, int tread_frame, bool people) {
    Bitmap b(96, 44);
    const Color tire{0x18, 0x18, 0x1c}, tread{0x60, 0x60, 0x6a};
    const Color chrome{0x9a, 0x9a, 0xa8}, grille{0x14, 0x14, 0x18}, slat{0x3a, 0x3a, 0x42};
    // Tail lights glow; brake lights burn, white-hot in the middle.
    const Color lamp = brake ? Color{0xff, 0x54, 0x3c} : Color{0x8c, 0x12, 0x12};
    const Color lamp_hi = brake ? Color{0xff, 0xf0, 0xe0} : Color{0xc8, 0x44, 0x38};
    const Color glow{0xff, 0x30, 0x20};

    // Shadow and tyres stay planted while the body shifts into the turn.
    paint::ellipse(b, 48.f, 41.f, 46.f, 3.f, Color{0x22, 0x22, 0x22});
    for (int side = 0; side < 2; ++side) {
        const int x = side ? 78 : 4;
        paint::rect(b, x, 27, 14, 15, tire);
        for (int y = 28 + tread_frame; y < 41; y += 3) paint::rect(b, x + 2, y, 10, 1, tread);
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
    paint::rect(b, 13 + s, 23, 18, brake ? 2 : 1, lamp_hi);
    paint::rect(b, 65 + s, 23, 18, brake ? 2 : 1, lamp_hi);
    if (brake) {
        // Light spilling onto the bodywork around the lamps.
        for (int x0 : {12, 64}) {
            for (int x = x0 - 1; x <= x0 + 20; ++x) {
                for (int y : {21, 28}) {
                    if (!b.inside(x + s, y)) continue;
                    const uint32_t p = b.get(x + s, y);
                    const Color under{static_cast<uint8_t>(p >> 16), static_cast<uint8_t>(p >> 8), static_cast<uint8_t>(p)};
                    b.set(x + s, y, blend(under, glow, 0.55f));
                }
            }
        }
    }
    // Indicators at the outer ends of the tail lights.
    for (int side = -1; side <= 1; side += 2) {
        const int x = (side < 0 ? 12 : 80) + s;
        const bool on = lit(signal, side);
        paint::rect(b, x, 22, 4, 6, on ? Color{0xff, 0xc0, 0x38} : Color{0xa8, 0x5c, 0x18});
        if (on) paint::rect(b, x + 1, 23, 2, 2, Color{0xff, 0xf4, 0xc0});
    }
    paint::rect(b, 34 + s, 22, 28, 9, grille);
    for (int y = 23; y < 31; y += 2) paint::rect(b, 35 + s, y, 26, 1, slat);
    paint::rect(b, 40 + s, 31, 16, 4, Color{0xe8, 0xe8, 0xd8});
    // Third brake light on the rear deck.
    paint::rect(b, 40 + s, 15, 16, 2, brake ? lamp : Color{0x70, 0x14, 0x12});
    if (brake) paint::rect(b, 42 + s, 15, 12, 1, lamp_hi);
    paint::rect(b, 43 + s, 32, 10, 1, Color{0x30, 0x30, 0x60});
    paint::ellipse(b, 24.f + s, 38.5f, 2.5f, 1.5f, Color{0x30, 0x30, 0x34});
    paint::ellipse(b, 72.f + s, 38.5f, 2.5f, 1.5f, Color{0x30, 0x30, 0x34});

    const int u = 2 * turn; // cabin leans further than the body
    if (style.convertible) {
        if (people) {
            const Color rest_dark{0x20, 0x20, 0x24}, rest{0x3c, 0x3c, 0x44}, rest_light{0x60, 0x60, 0x6a};
            paint::shaded_ellipse(b, 60.f + u, 8.f, 6.f, 7.f, Color{0xb0, 0x80, 0x30}, Color{0xe0, 0xb4, 0x50},
                                  Color{0xf8, 0xe0, 0x90});
            paint::shaded_ellipse(b, 36.f + u, 6.f, 5.f, 5.f, Color{0x3a, 0x22, 0x14}, Color{0x5c, 0x38, 0x1c},
                                  Color{0x84, 0x58, 0x2c});
            paint::shaded_ellipse(b, 36.f + u, 12.f, 6.f, 4.f, rest_dark, rest, rest_light);
            paint::shaded_ellipse(b, 60.f + u, 12.f, 6.f, 4.f, rest_dark, rest, rest_light);
        }
        paint::stroke(b, 21.f + u, 15.f, 28.f + u, 7.f, 1.5f, 1.5f, chrome);
        paint::stroke(b, 75.f + u, 15.f, 68.f + u, 7.f, 1.5f, 1.5f, chrome);
        paint::stroke(b, 28.f + u, 7.f, 68.f + u, 7.f, 1.f, 1.f, chrome);
    } else {
        paint::rect(b, 22 + u, 3, 52, 12, style.body);
        paint::rect(b, 24 + u, 2, 48, 1, style.body_light);
        paint::rect(b, 26 + u, 5, 44, 8, Color{0x2c, 0x3c, 0x54});
        if (people) {
            // Who sits inside follows from the paint job.
            const int k = style.body.r + 3 * style.body.g + 7 * style.body.b;
            Bitmap heads(b.w, b.h);
            heads_behind_glass(heads, static_cast<float>(u), 0, driver(k % drivers), passenger(k / 7 % passengers));
            for (size_t i = 0; i < heads.px.size(); ++i) {
                if (heads.px[i] >> 24) b.px[i] = heads.px[i];
            }
        }
        paint::stroke(b, 30.f + u, 12.f, 36.f + u, 5.f, 1.f, 1.f, Color{0x70, 0x88, 0xa8});
    }

    paint::outline(b, Outline);
    return b;
}

Bitmap make_car_front(const CarStyle& style, int signal, int tread_frame) {
    Bitmap b(96, 44);
    const Color tire{0x18, 0x18, 0x1c}, tread{0x60, 0x60, 0x6a};
    const Color chrome{0x9a, 0x9a, 0xa8}, grille{0x14, 0x14, 0x18}, slat{0x3a, 0x3a, 0x42};
    const Color lamp{0xf0, 0xec, 0xc8}, lamp_hi{0xff, 0xff, 0xff};
    const Color glass{0x2c, 0x3c, 0x54}, shine{0x70, 0x88, 0xa8};

    paint::ellipse(b, 48.f, 41.f, 46.f, 3.f, Color{0x22, 0x22, 0x22});
    for (int side = 0; side < 2; ++side) {
        const int x = side ? 78 : 4;
        paint::rect(b, x, 27, 14, 15, tire);
        for (int y = 28 + tread_frame; y < 41; y += 3) paint::rect(b, x + 2, y, 10, 1, tread);
    }

    // Body and bonnet, with the bumper and air intake below the lamps.
    paint::ellipse(b, 15.f, 27.f, 9.f, 9.f, style.body);
    paint::ellipse(b, 81.f, 27.f, 9.f, 9.f, style.body);
    paint::rect(b, 8, 19, 80, 18, style.body);
    paint::rect(b, 10, 15, 76, 5, style.body_light);
    paint::rect(b, 12, 14, 72, 1, style.body_light);
    paint::rect(b, 9, 31, 78, 6, style.body_dark);
    paint::rect(b, 30, 33, 36, 4, grille);

    // Headlights and indicators.
    paint::rect(b, 11, 21, 21, 6, lamp);
    paint::rect(b, 64, 21, 21, 6, lamp);
    paint::rect(b, 12, 22, 8, 2, lamp_hi);
    paint::rect(b, 65, 22, 8, 2, lamp_hi);
    for (int side = -1; side <= 1; side += 2) {
        const int x = side < 0 ? 11 : 79;
        if (lit(signal, side)) {
            paint::rect(b, x, 27, 6, 3, Color{0xff, 0xc0, 0x38});
            paint::rect(b, x + 2, 28, 2, 1, Color{0xff, 0xf4, 0xc0});
        } else {
            paint::rect(b, x, 28, 6, 2, Color{0xb8, 0x6c, 0x1c});
        }
    }

    // Grille with chrome surround, number plate below.
    paint::rect(b, 36, 21, 24, 9, chrome);
    paint::rect(b, 37, 22, 22, 7, grille);
    for (int y = 23; y < 29; y += 2) paint::rect(b, 38, y, 20, 1, slat);
    paint::rect(b, 41, 31, 14, 3, Color{0xe8, 0xe8, 0xd8});

    // Windscreen with the driver behind it. Seen from the front the driver of
    // a left-hand drive car sits on the right; the mirror puts them back left.
    if (style.convertible) {
        paint::stroke(b, 24.f, 14.f, 28.f, 6.f, 1.5f, 1.5f, chrome);
        paint::stroke(b, 72.f, 14.f, 68.f, 6.f, 1.5f, 1.5f, chrome);
        paint::stroke(b, 28.f, 6.f, 68.f, 6.f, 1.f, 1.f, chrome);
    } else {
        paint::rect(b, 20, 3, 56, 12, style.body);
        paint::rect(b, 22, 2, 52, 1, style.body_light);
        paint::rect(b, 23, 5, 50, 9, glass);
        paint::shaded_ellipse(b, 37.f, 9.f, 4.f, 4.f, Color{0x14, 0x14, 0x1c}, Color{0x24, 0x24, 0x30},
                              Color{0x34, 0x34, 0x40});
        paint::rect(b, 32, 12, 10, 2, Color{0x1c, 0x1c, 0x26});
        paint::stroke(b, 59.f, 13.f, 65.f, 6.f, 1.f, 1.f, shine);
    }

    paint::outline(b, Outline);
    return b;
}

Bitmap make_player_car(const CarStyle& style, int turn, bool brake, int signal, int tread, int headroom) {
    const Bitmap car = make_car(style, turn, signal, brake, tread, false);
    Bitmap b(car.w, car.h + headroom);
    std::copy(car.px.begin(), car.px.end(), b.px.begin() + static_cast<std::ptrdiff_t>(headroom) * car.w);
    return b;
}

namespace {

// A head from behind, centred at (x, y), radius r.
void draw_head(Bitmap& b, float x, float y, float r, const Person& p) {
    const float ry = p.style == HeadStyle::Long ? r * 1.35f : r;
    switch (p.style) {
        case HeadStyle::Bald:
            paint::shaded_ellipse(b, x, y, r, r, p.hair_dark, p.hair, p.hair_light);
            break;
        case HeadStyle::Mohawk:
            paint::shaded_ellipse(b, x, y, r, r, blend(p.skin, Color{0, 0, 0}, 0.25f), p.skin, blend(p.skin, Color{255, 255, 255}, 0.3f));
            paint::rect(b, static_cast<int>(x) - 1, static_cast<int>(y - r) - 3, 3, static_cast<int>(r) + 4, p.hair);
            paint::rect(b, static_cast<int>(x) - 1, static_cast<int>(y - r) - 3, 1, static_cast<int>(r) + 4, p.hair_light);
            break;
        case HeadStyle::Helmet:
            paint::shaded_ellipse(b, x, y, r + 1.f, r + 1.f, p.hair_dark, p.hair, p.hair_light);
            paint::rect(b, static_cast<int>(x) - 1, static_cast<int>(y - r) - 1, 2, static_cast<int>(2.f * r) + 2, p.accent);
            break;
        case HeadStyle::Cap:
            paint::shaded_ellipse(b, x, y, r, r, p.hair_dark, p.hair, p.hair_light);
            paint::shaded_ellipse(b, x, y - r * 0.45f, r + 0.5f, r * 0.6f, blend(p.accent, Color{0, 0, 0}, 0.3f), p.accent,
                                  blend(p.accent, Color{255, 255, 255}, 0.3f));
            break;
        case HeadStyle::Bun:
            paint::shaded_ellipse(b, x, y, r, r, p.hair_dark, p.hair, p.hair_light);
            paint::shaded_ellipse(b, x, y - r - 1.5f, r * 0.5f, r * 0.45f, p.hair_dark, p.hair, p.hair_light);
            break;
        case HeadStyle::Dog:
            paint::shaded_ellipse(b, x, y, r, r * 0.9f, p.hair_dark, p.hair, p.hair_light);
            for (float side : {-1.f, 1.f}) {
                paint::shaded_ellipse(b, x + side * r, y + 1.f, 2.f, r * 0.8f, p.hair_dark, p.hair_dark, p.hair);
            }
            break;
        default:
            paint::shaded_ellipse(b, x, y, r, ry, p.hair_dark, p.hair, p.hair_light);
            break;
    }
}

// A bandage wound round a head of radius r at (x, y), a spot of red on it.
void bandage(Bitmap& b, float x, float y, float r) {
    const Color gauze{0xf4, 0xf4, 0xec}, shade{0xc8, 0xc8, 0xc0};
    const int x0 = static_cast<int>(x - r), w = static_cast<int>(2.f * r) + 1, y0 = static_cast<int>(y - r * 0.4f);
    paint::rect(b, x0, y0, w, 3, gauze);
    paint::rect(b, x0, y0 + 2, w, 1, shade);
    paint::rect(b, static_cast<int>(x + r * 0.3f), y0, 2, 2, Color{0xd0, 0x20, 0x20});
}

// The two of them seen through a closed car's rear window, `top` pixels down
// in `b`, whose other pixels must still be transparent: their heads, clipped
// to the glass and lightly tinted by it.
void heads_behind_glass(Bitmap& b, float u, int top, const Person& driver, const Person& passenger, bool bandaged) {
    const float t = static_cast<float>(top);
    draw_head(b, 37.f + u, t + 10.5f, 4.5f, driver);
    if (bandaged) bandage(b, 37.f + u, t + 10.5f, 4.5f);
    draw_head(b, 59.f + u, t + 10.5f, 4.5f, passenger);
    const Color glass{0x2c, 0x3c, 0x54};
    const int x0 = 26 + static_cast<int>(u), y0 = top + 5;
    for (int y = 0; y < b.h; ++y) {
        for (int x = 0; x < b.w; ++x) {
            uint32_t& p = b.px[static_cast<size_t>(y) * b.w + x];
            if (!(p >> 24)) continue;
            if (x < x0 || x >= x0 + 44 || y < y0 || y >= y0 + 8) { p = 0u; continue; }
            const Color c{static_cast<uint8_t>(p >> 16), static_cast<uint8_t>(p >> 8), static_cast<uint8_t>(p)};
            p = blend(c, glass, 0.25f).argb();
        }
    }
}

} // namespace

Bitmap make_occupants(const Person& driver, const Person& passenger, int turn, int wave, int frame, bool convertible,
                      int headroom, bool bandaged) {
    Bitmap b(96, 44 + headroom);
    const float u = static_cast<float>(2 * turn); // the cabin leans like in make_car
    const float h = static_cast<float>(headroom);
    if (convertible) {
        // Heads above the seats, the headrests in front of their necks.
        const Color rest_dark{0x20, 0x20, 0x24}, rest{0x3c, 0x3c, 0x44}, rest_light{0x60, 0x60, 0x6a};
        draw_head(b, 60.f + u, h + 7.f, 6.f, passenger);
        draw_head(b, 36.f + u, h + 6.f, 5.f, driver);
        if (bandaged) bandage(b, 36.f + u, h + 6.f, 5.f);
        paint::shaded_ellipse(b, 36.f + u, h + 12.f, 6.f, 4.f, rest_dark, rest, rest_light);
        paint::shaded_ellipse(b, 60.f + u, h + 12.f, 6.f, 4.f, rest_dark, rest, rest_light);
        paint::outline(b, Outline);
    } else {
        heads_behind_glass(b, u, headroom, driver, passenger, bandaged);
        // The glint on the glass stays in front of them.
        paint::stroke(b, 30.f + u, h + 12.f, 36.f + u, h + 5.f, 1.f, 1.f, Color{0x70, 0x88, 0xa8});
    }
    if (wave == 0) return b;

    // A raised arm and open hand (or paw), waving: outlined on its own.
    const Person& waver = wave < 0 ? driver : passenger;
    const Color skin = waver.skin, skin_dark = blend(waver.skin, Color{0, 0, 0}, 0.25f);
    // The shoulder: in a convertible above the seat, in a closed car at the
    // side window.
    const float sx = convertible ? (wave < 0 ? 30.f : 66.f) + u : (wave < 0 ? 22.f : 74.f) + u;
    const float sy = convertible ? h + 11.f : h + 9.f;
    const float out = static_cast<float>(wave) * (frame ? 4.f : 8.f); // lean of the wave
    const float hx = sx + out, hy = h - 7.f + (frame ? 0.f : 1.f);    // hand
    Bitmap arm(b.w, b.h);
    paint::stroke(arm, sx, sy, hx, hy + 3.f, 2.6f, 2.f, skin);
    paint::stroke(arm, sx + 1.f, sy, hx + 1.f, hy + 3.f, 1.f, 1.f, skin_dark);
    paint::ellipse(arm, hx, hy, 2.5f, 3.f, skin);
    paint::rect(arm, static_cast<int>(hx) - (wave < 0 ? 4 : -3), static_cast<int>(hy), 2, 1, skin); // thumb
    paint::outline(arm, Outline);
    for (size_t i = 0; i < arm.px.size(); ++i) {
        if (arm.px[i] >> 24) b.px[i] = arm.px[i];
    }
    return b;
}

namespace {

// A tail light panel; braking it burns brighter, white-hot in the middle.
void tail_lamp(Bitmap& b, int x, int y, int w, int h, bool brake) {
    paint::rect(b, x, y, w, h, brake ? Color{0xff, 0x54, 0x3c} : Color{0x8c, 0x12, 0x12});
    if (w > 2 && h > 2) {
        paint::rect(b, x + 1, y + 1, w - 2, std::max(1, h / 4),
                    brake ? Color{0xff, 0xf0, 0xe0} : Color{0xc8, 0x44, 0x38});
    }
}

void indicator(Bitmap& b, int x, int y, int w, int h, bool on) {
    paint::rect(b, x, y, w, h, on ? Color{0xff, 0xc0, 0x38} : Color{0xa8, 0x5c, 0x18});
    if (on && w > 2 && h > 2) paint::rect(b, x + 1, y + 1, w - 2, h - 2, Color{0xff, 0xf4, 0xc0});
}

void headlight(Bitmap& b, int x, int y, int w, int h) {
    paint::rect(b, x, y, w, h, Color{0xf0, 0xec, 0xc8});
    paint::rect(b, x + 1, y + 1, std::max(1, w / 2), std::max(1, h / 2), Color{0xff, 0xff, 0xff});
}

// A tyre with tread rows; `tread` (0 .. 2) shifts them down a pixel per
// frame, so cycling through the frames makes the tyre roll.
void tyre(Bitmap& b, int x, int y, int w, int h, int tread) {
    paint::rect(b, x, y, w, h, Color{0x18, 0x18, 0x1c});
    for (int ty = y + 1 + tread; ty < y + h - 1; ty += 3) paint::rect(b, x + 2, ty, w - 4, 1, Color{0x60, 0x60, 0x6a});
}

void driver(Bitmap& b, float x, float y) {
    paint::shaded_ellipse(b, x, y, 4.f, 4.f, Color{0x14, 0x14, 0x1c}, Color{0x24, 0x24, 0x30}, Color{0x34, 0x34, 0x40});
}

} // namespace

// A panel van: tall box body, two rear doors with windows, tall tail lights.
Bitmap make_van(const CarStyle& st, int signal, bool brake, int tread) {
    Bitmap b(104, 64);
    paint::ellipse(b, 52.f, 61.f, 50.f, 3.f, Color{0x22, 0x22, 0x22});
    tyre(b, 6, 48, 15, 16, tread);
    tyre(b, 83, 48, 15, 16, tread);
    paint::rect(b, 6, 4, 92, 1, st.body_light);
    paint::rect(b, 4, 5, 96, 50, st.body);
    paint::rect(b, 4, 5, 96, 4, st.body_light);
    paint::rect(b, 4, 46, 96, 9, st.body_dark);
    paint::rect(b, 51, 9, 2, 37, st.body_dark); // door seam
    const Color glass{0x2c, 0x3c, 0x54}, shine{0x70, 0x88, 0xa8}, chrome{0x9a, 0x9a, 0xa8};
    for (int x : {14, 56}) {
        paint::rect(b, x, 11, 34, 15, glass);
        paint::stroke(b, static_cast<float>(x) + 4.f, 24.f, static_cast<float>(x) + 10.f, 13.f, 1.f, 1.f, shine);
    }
    paint::rect(b, 46, 32, 4, 2, chrome);
    paint::rect(b, 54, 32, 4, 2, chrome);
    for (int side = -1; side <= 1; side += 2) {
        const int x = side < 0 ? 5 : 93;
        indicator(b, x, 25, 6, 6, lit(signal, side));
        tail_lamp(b, x, 31, 6, 14, brake);
    }
    paint::rect(b, 44, 8, 16, 2, brake ? Color{0xff, 0x54, 0x3c} : Color{0x70, 0x14, 0x12}); // third brake light
    paint::rect(b, 3, 53, 98, 5, Color{0x4c, 0x4c, 0x54}); // bumper
    paint::rect(b, 44, 47, 16, 5, Color{0xe8, 0xe8, 0xd8});
    paint::rect(b, 47, 48, 10, 1, Color{0x30, 0x30, 0x60});
    paint::outline(b, Outline);
    return b;
}

Bitmap make_van_front(const CarStyle& st, int signal, int tread) {
    Bitmap b(104, 64);
    paint::ellipse(b, 52.f, 61.f, 50.f, 3.f, Color{0x22, 0x22, 0x22});
    tyre(b, 6, 48, 15, 16, tread);
    tyre(b, 83, 48, 15, 16, tread);
    paint::rect(b, 4, 4, 96, 51, st.body);
    paint::rect(b, 4, 4, 96, 3, st.body_light);
    paint::rect(b, 10, 8, 84, 22, Color{0x2c, 0x3c, 0x54}); // windscreen
    driver(b, 30.f, 19.f);
    paint::rect(b, 25, 23, 10, 6, Color{0x1c, 0x1c, 0x26});
    paint::stroke(b, 66.f, 27.f, 74.f, 11.f, 1.f, 1.f, Color{0x70, 0x88, 0xa8});
    paint::rect(b, 4, 30, 96, 3, st.body_dark);
    headlight(b, 8, 35, 18, 7);
    headlight(b, 78, 35, 18, 7);
    for (int side = -1; side <= 1; side += 2) indicator(b, side < 0 ? 8 : 90, 43, 6, 3, lit(signal, side));
    paint::rect(b, 32, 35, 40, 9, Color{0x14, 0x14, 0x18}); // grille
    for (int y = 36; y < 44; y += 2) paint::rect(b, 33, y, 38, 1, Color{0x3a, 0x3a, 0x42});
    paint::rect(b, 3, 49, 98, 6, Color{0x4c, 0x4c, 0x54});
    paint::rect(b, 44, 49, 16, 4, Color{0xe8, 0xe8, 0xd8});
    paint::outline(b, Outline);
    return b;
}

// A truck from behind: the doors of a ribbed box trailer with lock bars, the
// company's name, lights low down, an under-run bar, mud flaps, twin tyres.
Bitmap make_truck(const CarStyle& st, int signal, bool brake, int tread) {
    Bitmap b(112, 104);
    paint::ellipse(b, 56.f, 101.f, 54.f, 3.f, Color{0x22, 0x22, 0x22});
    tyre(b, 4, 86, 22, 18, tread);
    tyre(b, 86, 86, 22, 18, tread);
    paint::rect(b, 2, 0, 108, 84, st.body);
    paint::rect(b, 2, 0, 108, 3, st.body_light);
    for (int x = 8; x < 108; x += 9) paint::rect(b, x, 3, 1, 79, st.body_dark); // ribs
    paint::rect(b, 55, 3, 2, 79, Color{0x30, 0x30, 0x34}); // door seam
    const Color chrome{0xb0, 0xb0, 0xbc};
    for (int x : {30, 46, 64, 80}) {
        paint::rect(b, x, 6, 2, 74, chrome);
        paint::rect(b, x - 2, 40, 6, 2, chrome);
    }
    paint::rect(b, 2, 82, 108, 4, Color{0x30, 0x30, 0x34});
    for (int x : {6, 104}) paint::rect(b, x, 1, 2, 2, Color{0xff, 0xa0, 0x30}); // marker lights
    paint::rect(b, 2, 86, 108, 4, Color{0x26, 0x26, 0x2a}); // chassis
    for (int x = 28; x < 84; x += 8) paint::rect(b, x, 90, 4, 3, (x / 8) % 2 ? Color{0xe8, 0x20, 0x20} : Color{0xf0, 0xf0, 0xf0});
    paint::rect(b, 24, 93, 64, 1, Color{0x30, 0x30, 0x34}); // under-run bar
    for (int side = -1; side <= 1; side += 2) {
        const int x = side < 0 ? 6 : 88;
        indicator(b, side < 0 ? x : x + 12, 84, 6, 4, lit(signal, side));
        tail_lamp(b, side < 0 ? x + 6 : x, 84, 12, 4, brake);
        paint::rect(b, side < 0 ? 26 : 80, 94, 6, 10, Color{0x14, 0x14, 0x14}); // mud flaps
    }
    paint::rect(b, 48, 90, 16, 5, Color{0xe8, 0xe8, 0xd8});
    paint::outline(b, Outline);
    return b;
}

Bitmap make_truck_front(const CarStyle& st, int signal, int tread) {
    Bitmap b(112, 104);
    paint::ellipse(b, 56.f, 101.f, 54.f, 3.f, Color{0x22, 0x22, 0x22});
    tyre(b, 8, 84, 18, 20, tread);
    tyre(b, 86, 84, 18, 20, tread);
    paint::rect(b, 6, 0, 100, 14, Color{0xc8, 0xc8, 0xd0}); // the trailer above the cab
    paint::rect(b, 8, 10, 96, 76, st.body);
    paint::rect(b, 8, 10, 96, 6, st.body_dark); // sun visor
    paint::rect(b, 12, 17, 88, 28, Color{0x2c, 0x3c, 0x54});
    paint::rect(b, 55, 17, 2, 28, st.body); // split windscreen
    driver(b, 32.f, 30.f);
    paint::rect(b, 26, 35, 12, 9, Color{0x1c, 0x1c, 0x26});
    paint::stroke(b, 70.f, 42.f, 80.f, 20.f, 1.f, 1.f, Color{0x70, 0x88, 0xa8});
    const Color chrome{0xb0, 0xb0, 0xbc};
    paint::rect(b, 30, 50, 52, 26, chrome); // grille
    for (int y = 52; y < 75; y += 3) paint::rect(b, 32, y, 48, 1, Color{0x50, 0x50, 0x58});
    headlight(b, 12, 66, 14, 7);
    headlight(b, 86, 66, 14, 7);
    for (int side = -1; side <= 1; side += 2) indicator(b, side < 0 ? 12 : 94, 58, 6, 5, lit(signal, side));
    paint::rect(b, 6, 78, 100, 7, Color{0x3c, 0x3c, 0x44});
    paint::rect(b, 46, 79, 20, 5, Color{0xe8, 0xe8, 0xd8});
    paint::outline(b, Outline);
    return b;
}

// The rival: a low, wide sports car with a big rear wing, racing stripes,
// four round tail lights and twin exhausts.
Bitmap make_rival(const CarStyle& st, int signal, bool brake, int tread) {
    Bitmap b(100, 40);
    paint::ellipse(b, 50.f, 37.f, 49.f, 3.f, Color{0x22, 0x22, 0x22});
    tyre(b, 2, 24, 18, 16, tread);
    tyre(b, 80, 24, 18, 16, tread);
    paint::ellipse(b, 14.f, 24.f, 11.f, 10.f, st.body);
    paint::ellipse(b, 86.f, 24.f, 11.f, 10.f, st.body);
    paint::rect(b, 6, 15, 88, 18, st.body);
    paint::rect(b, 14, 11, 72, 5, st.body);
    paint::rect(b, 30, 9, 40, 6, Color{0x1c, 0x1c, 0x26}); // low cabin, tinted
    paint::rect(b, 24, 5, 2, 7, Color{0x20, 0x20, 0x24}); // wing uprights
    paint::rect(b, 74, 5, 2, 7, Color{0x20, 0x20, 0x24});
    paint::rect(b, 8, 2, 84, 4, st.body_dark); // wing
    for (int x : {42, 54}) {
        paint::rect(b, x, 2, 4, 4, st.body_light); // stripes over wing and deck
        paint::rect(b, x, 15, 4, 9, st.body_light);
    }
    for (int x : {16, 28, 72, 84}) {
        const Color lamp = brake ? Color{0xff, 0x54, 0x3c} : Color{0x8c, 0x12, 0x12};
        paint::ellipse(b, static_cast<float>(x), 21.f, 3.5f, 3.f, lamp);
        if (brake) paint::rect(b, x - 1, 20, 2, 2, Color{0xff, 0xf0, 0xe0});
    }
    indicator(b, 7, 19, 3, 4, lit(signal, -1));
    indicator(b, 90, 19, 3, 4, lit(signal, 1));
    paint::rect(b, 20, 30, 60, 5, Color{0x14, 0x14, 0x18}); // diffuser
    for (int x = 24; x < 78; x += 6) paint::rect(b, x, 30, 1, 5, Color{0x40, 0x40, 0x48});
    for (float x : {44.f, 56.f}) paint::ellipse(b, x, 31.f, 2.5f, 2.f, Color{0xb0, 0xb0, 0xbc});
    paint::rect(b, 42, 25, 16, 4, Color{0xe8, 0xe8, 0xd8});
    paint::outline(b, Outline);
    return b;
}

namespace {

// A police car's lightbar on the roof, red on the left and blue on the
// right; `lit` -1 flashes the red, +1 the blue, 0 neither.
void lightbar(Bitmap& b, int lit) {
    const Color red_off{0x70, 0x10, 0x14}, blue_off{0x10, 0x20, 0x70};
    const Color red{0xff, 0x30, 0x30}, blue{0x40, 0x70, 0xff}, core{0xff, 0xf8, 0xf0};
    paint::rect(b, 30, 0, 36, 3, Color{0x30, 0x30, 0x34});
    paint::rect(b, 31, 0, 16, 2, lit < 0 ? red : red_off);
    paint::rect(b, 49, 0, 16, 2, lit > 0 ? blue : blue_off);
    if (lit != 0) paint::rect(b, lit < 0 ? 35 : 53, 0, 8, 1, core);
}

} // namespace

Bitmap make_police(const CarStyle& style, int lights, bool brake, int tread) {
    Bitmap b = make_car(style, 0, 0, brake, tread);
    lightbar(b, lights);
    return b;
}

Bitmap make_police_front(const CarStyle& style, int lights, int tread) {
    Bitmap b = make_car_front(style, 0, tread);
    lightbar(b, lights);
    return b;
}

Bitmap make_player_police(const CarStyle& style, int turn, bool brake, int signal, int tread, int headroom) {
    Bitmap b = make_player_car(style, turn, brake, signal, tread, headroom);
    // The lightbar on the roof, off: in the headroom, as the roof sits there.
    Bitmap bar(b.w, 3);
    lightbar(bar, 0);
    for (int y = 0; y < 3; ++y) {
        for (int x = 0; x < b.w; ++x) {
            const uint32_t p = bar.px[static_cast<size_t>(y) * bar.w + x];
            if (p >> 24) b.px[static_cast<size_t>(headroom - 1 + y) * b.w + static_cast<size_t>(x + 2 * turn)] = p;
        }
    }
    return b;
}

void taxi_sign(Bitmap& b, int lean, int headroom) {
    const int x = 36 + lean, y = headroom - 6;
    paint::rect(b, x, y, 24, 7, Color{0xf8, 0xe0, 0x40});
    paint::rect(b, x + 1, y + 1, 22, 5, Color{0x18, 0x18, 0x18});
    paint::text(b, x + 1, y, "TAXI", Color{0xf8, 0xe0, 0x40});
    paint::rect(b, x - 1, y - 1, 26, 1, Outline);
    paint::rect(b, x - 1, y + 7, 26, 1, Outline);
}

Bitmap make_player_truck(const CarStyle& style, int turn, bool brake, int signal, int tread, int headroom) {
    Bitmap b(96, 44 + headroom);
    const int h = b.h;
    const Color frame{0x24, 0x24, 0x28}, steel{0x50, 0x50, 0x58}, chrome{0xb0, 0xb0, 0xbc}, chrome_dark{0x70, 0x70, 0x7c};
    paint::ellipse(b, 48.f, static_cast<float>(h) - 3.f, 46.f, 3.f, Color{0x22, 0x22, 0x22});
    // Twin wheels each side, planted; mudflaps hanging behind their lower half.
    for (int x : {3, 12, 70, 79}) tyre(b, x, h - 18, 9, 17, tread);
    for (int x : {4, 72}) {
        paint::rect(b, x, h - 9, 20, 7, Color{0x14, 0x14, 0x18});
        paint::rect(b, x + 2, h - 8, 16, 1, chrome_dark);
    }
    // The chassis: frame rails, the fifth-wheel plate and the rear bar.
    paint::rect(b, 22, h - 24, 52, 12, frame);
    paint::shaded_ellipse(b, 48.f, static_cast<float>(h) - 23.f, 18.f, 4.f, frame, steel, chrome_dark);
    paint::rect(b, 14, h - 14, 68, 5, steel);
    paint::rect(b, 14, h - 14, 68, 1, chrome);
    const Color lamp = brake ? Color{0xff, 0x54, 0x3c} : Color{0x8c, 0x12, 0x12};
    for (int x : {16, 70}) {
        paint::rect(b, x, h - 14, 10, 5, lamp);
        if (brake) paint::rect(b, x + 1, h - 13, 8, 2, Color{0xff, 0xf0, 0xe0});
    }
    paint::rect(b, 42, h - 13, 12, 4, Color{0xe8, 0xe8, 0xd8}); // plate
    indicator(b, 10, h - 14, 4, 5, lit(signal, -1));
    indicator(b, 82, h - 14, 4, 5, lit(signal, 1));

    // The cab, leaning into turns, and its back wall with the window.
    const int u = 2 * turn;
    paint::rect(b, 18 + u, 4, 60, h - 26, style.body);
    paint::rect(b, 18 + u, 4, 2, h - 26, style.body_light);
    paint::rect(b, 76 + u, 4, 2, h - 26, style.body_dark);
    paint::rect(b, 18 + u, h - 28, 60, 4, style.body_dark);
    for (int y = headroom + 15; y < h - 28; y += 3) paint::rect(b, 22 + u, y, 52, 1, style.body_dark); // ribs
    // The air deflector on the roof, narrower towards the top.
    for (int y = 0; y < 4; ++y) paint::rect(b, 26 + u - 2 * y, y, 44 + 4 * y, 1, y ? style.body : style.body_light);
    for (int x : {30, 46, 62}) paint::rect(b, x + u, 5, 4, 2, Color{0xff, 0xb0, 0x30}); // marker lamps
    paint::rect(b, 26 + u, headroom + 5, 44, 8, Color{0x2c, 0x3c, 0x54});
    paint::stroke(b, 30.f + u, static_cast<float>(headroom) + 12.f, 36.f + u, static_cast<float>(headroom) + 5.f, 1.f, 1.f,
                  Color{0x70, 0x88, 0xa8});
    // Exhaust stacks up both sides.
    for (int x : {11, 81}) {
        paint::rect(b, x + turn, 0, 4, h - 20, chrome);
        paint::rect(b, x + turn + 3, 0, 1, h - 20, chrome_dark);
        paint::rect(b, x + turn, 0, 4, 2, Color{0x20, 0x20, 0x20});
    }
    paint::outline(b, Outline);
    return b;
}

Bitmap make_dashboard(const CarStyle& style, int width) {
    Bitmap b(width, dashboard_height);
    const Color dash{0x26, 0x24, 0x2a}, dash_light{0x3c, 0x3a, 0x42}, dash_dark{0x16, 0x14, 0x18};
    const float half = static_cast<float>(width) / 2.f;
    for (int x = 0; x < width; ++x) {
        // The dash's top edge sags towards the sides; the instrument hood
        // rises over the wheel.
        const float d = (static_cast<float>(x) - half) / half;
        const float hood = static_cast<float>(x - dashboard_wheel_x) / 42.f;
        int top = 14 + static_cast<int>(8.f * d * d);
        if (hood > -1.f && hood < 1.f) top = std::min(top, 2 + static_cast<int>(10.f * hood * hood));
        // The body-coloured cowl ahead of the dash, the windscreen's base.
        paint::rect(b, x, std::max(0, top - 3), 1, 3, style.body);
        paint::rect(b, x, top, 1, dashboard_height - top, dash);
        paint::rect(b, x, top, 1, 1, dash_light);
    }
    // The two dials under the hood.
    for (int k = 0; k < 2; ++k) {
        const float cx = static_cast<float>(dashboard_wheel_x - 17 + 34 * k), cy = 18.f;
        paint::ellipse(b, cx, cy, 11.f, 11.f, dash_dark);
        paint::ellipse(b, cx, cy, 9.f, 9.f, Color{0x10, 0x10, 0x14});
        for (int t = 0; t < 7; ++t) {
            const float a = 2.4f + static_cast<float>(t) * 0.75f;
            paint::rect(b, static_cast<int>(cx + 7.f * std::cos(a)), static_cast<int>(cy + 7.f * std::sin(a)), 1, 1,
                        Color{0xe8, 0xe8, 0xe0});
        }
        paint::stroke(b, cx, cy, cx - 5.f, cy + 3.f, 1.f, 1.f, Color{0xff, 0x60, 0x30});
    }
    // The centre console: vents and the radio.
    const int cx = static_cast<int>(half) + 24;
    for (int i = 0; i < 2; ++i) {
        paint::rect(b, cx + i * 30, 26, 24, 10, dash_dark);
        for (int y = 28; y < 35; y += 2) paint::rect(b, cx + i * 30 + 1, y, 22, 1, dash_light);
    }
    paint::rect(b, cx + 6, 42, 42, 10, Color{0x10, 0x10, 0x14});
    paint::text(b, cx + 9, 44, "88.5", Color{0x60, 0xf0, 0x90});
    // The glovebox line on the passenger's side.
    paint::rect(b, width - 70, 40, 56, 1, dash_dark);
    return b;
}

Bitmap make_wheel(const Person& driver) {
    Bitmap b(wheel_size, wheel_size);
    const float c = static_cast<float>(wheel_size) / 2.f, r = c - 3.f;
    const Color rim{0x1c, 0x1a, 0x1e}, rim_light{0x4a, 0x46, 0x50}, spoke{0x5c, 0x5c, 0x66};
    // The rim: a thick ring, lit along the top.
    for (int y = 0; y < wheel_size; ++y) {
        for (int x = 0; x < wheel_size; ++x) {
            const float dx = static_cast<float>(x) + 0.5f - c, dy = static_cast<float>(y) + 0.5f - c;
            const float d = std::sqrt(dx * dx + dy * dy);
            if (d <= r && d >= r - 6.f) b.set(x, y, d > r - 2.f && dy < 0.f ? rim_light : rim);
        }
    }
    // Three spokes and the hub.
    for (float a : {0.f, 3.14159265f, 1.5707963f}) {
        paint::stroke(b, c, c, c + std::cos(a) * (r - 4.f), c + std::sin(a) * (r - 4.f), 5.f, 3.f, spoke);
    }
    paint::shaded_ellipse(b, c, c, 9.f, 9.f, rim, spoke, rim_light);
    paint::ellipse(b, c, c, 3.f, 3.f, Color{0xd0, 0x20, 0x20});
    // The hands at ten to two: knuckles on the rim, a thumb over it.
    const Color skin = driver.skin, skin_dark = blend(driver.skin, Color{0, 0, 0}, 0.3f);
    for (float side : {-1.f, 1.f}) {
        const float a = -1.5707963f + side * 0.95f;
        const float hx = c + std::cos(a) * (r - 3.f), hy = c + std::sin(a) * (r - 3.f);
        paint::shaded_ellipse(b, hx, hy, 6.f, 5.f, skin_dark, skin, blend(skin, Color{255, 255, 255}, 0.25f));
        paint::rect(b, static_cast<int>(hx - side * 4.f) - 1, static_cast<int>(hy) + 2, 3, 2, skin_dark);
    }
    paint::outline(b, Outline);
    return b;
}

Bitmap make_rival_front(const CarStyle& st, int signal, int tread) {
    Bitmap b(100, 40);
    paint::ellipse(b, 50.f, 37.f, 49.f, 3.f, Color{0x22, 0x22, 0x22});
    tyre(b, 2, 24, 18, 16, tread);
    tyre(b, 80, 24, 18, 16, tread);
    paint::ellipse(b, 14.f, 25.f, 11.f, 9.f, st.body);
    paint::ellipse(b, 86.f, 25.f, 11.f, 9.f, st.body);
    paint::rect(b, 6, 17, 88, 16, st.body);
    paint::rect(b, 22, 6, 56, 12, st.body);
    paint::rect(b, 26, 8, 48, 9, Color{0x1c, 0x1c, 0x26});
    paint::shaded_ellipse(b, 38.f, 12.f, 4.f, 4.f, Color{0xc0, 0x18, 0x18}, Color{0xe8, 0x30, 0x30}, Color{0xff, 0x80, 0x70}); // helmet
    for (int x : {42, 54}) paint::rect(b, x, 17, 4, 16, st.body_light);
    headlight(b, 8, 19, 18, 3);
    headlight(b, 74, 19, 18, 3);
    indicator(b, 8, 23, 6, 2, lit(signal, -1));
    indicator(b, 86, 23, 6, 2, lit(signal, 1));
    paint::rect(b, 14, 27, 20, 6, Color{0x14, 0x14, 0x18}); // intakes
    paint::rect(b, 66, 27, 20, 6, Color{0x14, 0x14, 0x18});
    paint::rect(b, 38, 28, 24, 5, Color{0x14, 0x14, 0x18});
    paint::outline(b, Outline);
    return b;
}

Bitmap make_app_icon() {
    constexpr int n = 32;
    Bitmap b(n, n);
    const Color sky_top{0x38, 0x24, 0x68}, sky_low{0xf4, 0x9a, 0x54}, sun{0xff, 0xd8, 0x70};
    const Color grass[2] = {{0x4a, 0xa0, 0x3a}, {0x3a, 0x80, 0x2a}};
    const Color road{0x68, 0x68, 0x70}, white{0xf0, 0xf0, 0xf0}, red{0xd0, 0x20, 0x20};
    constexpr int horizon = 14;

    // Sunset sky in dithered bands, the sun sitting on the horizon.
    for (int y = 0; y < horizon; ++y) {
        const float t = static_cast<float>(y) / static_cast<float>(horizon - 1) * 3.f;
        const int band = static_cast<int>(t);
        const Color c0 = blend(sky_top, sky_low, static_cast<float>(band) / 3.f);
        const Color c1 = blend(sky_top, sky_low, static_cast<float>(std::min(band + 1, 3)) / 3.f);
        for (int x = 0; x < n; ++x) b.set(x, y, bayer4(x, y) < t - static_cast<float>(band) ? c1 : c0);
    }
    for (int y = horizon - 6; y < horizon; ++y)
        for (int x = 10; x < 22; ++x)
            if (std::hypot(static_cast<float>(x) + 0.5f - 16.f, static_cast<float>(y) + 0.5f - static_cast<float>(horizon)) < 6.f)
                b.set(x, y, sun);

    // Grass and the road running to the vanishing point, with rumble strips
    // and a dashed centre line.
    for (int y = horizon; y < n; ++y) {
        const int band = ((n - y) / 3) % 2;
        for (int x = 0; x < n; ++x) b.set(x, y, grass[band]);
        const float t = static_cast<float>(y - horizon + 1) / static_cast<float>(n - horizon);
        const float half = 1.f + 14.f * t, rumble = std::max(1.f, 2.f * t);
        for (int x = 0; x < n; ++x) {
            const float d = std::abs(static_cast<float>(x) + 0.5f - 16.f);
            if (d < half) b.set(x, y, road);
            else if (d < half + rumble) b.set(x, y, band ? white : red);
        }
        if (band && y < n - 2) b.set(15, y, white), b.set(16, y, white);
    }

    // The car from behind.
    const Color body{0xd0, 0x18, 0x1c}, dark{0x88, 0x08, 0x10}, light{0xf0, 0x60, 0x50};
    const Color tyre{0x18, 0x18, 0x1c}, lamp{0xff, 0x50, 0x34}, glass{0x2c, 0x3c, 0x54};
    paint::rect(b, 9, 27, 3, 4, tyre);
    paint::rect(b, 20, 27, 3, 4, tyre);
    paint::rect(b, 12, 20, 8, 3, glass);
    paint::rect(b, 11, 20, 1, 3, body);
    paint::rect(b, 20, 20, 1, 3, body);
    paint::rect(b, 9, 23, 14, 5, body);
    paint::rect(b, 10, 22, 12, 1, light);
    paint::rect(b, 9, 27, 14, 1, dark);
    paint::rect(b, 10, 24, 3, 2, lamp);
    paint::rect(b, 19, 24, 3, 2, lamp);
    paint::rect(b, 14, 25, 4, 1, Color{0x20, 0x20, 0x24});
    // Outline the car (body, cabin, tyres) against the road.
    const auto is_car = [](int x, int y) {
        const bool body = x >= 9 && x < 23 && y >= 22 && y < 28;
        const bool cabin = x >= 11 && x < 21 && y >= 20 && y < 22;
        const bool tyres = ((x >= 9 && x < 12) || (x >= 20 && x < 23)) && y >= 27 && y < 31;
        return body || cabin || tyres;
    };
    for (int y = 19; y < 31; ++y) {
        for (int x = 8; x < 24; ++x) {
            if (!is_car(x, y) && (is_car(x - 1, y) || is_car(x + 1, y) || is_car(x, y - 1) || is_car(x, y + 1))) {
                b.set(x, y, Outline);
            }
        }
    }

    // Rounded corners and a dark rim.
    for (int y = 0; y < n; ++y) {
        for (int x = 0; x < n; ++x) {
            const int dx = std::min(x, n - 1 - x), dy = std::min(y, n - 1 - y);
            if (dx + dy < 2) b.px[static_cast<size_t>(y) * n + x] = 0u;
            else if (dx == 0 || dy == 0 || dx + dy == 2) b.set(x, y, Outline);
        }
    }
    return b;
}

const Bitmap& SpriteSheet::occupants(int driver_index, int passenger_index, int steer, int wave, int frame,
                                     int model, bool bandaged) const {
    const bool convertible = player_convertible(model);
    const auto d = static_cast<uint32_t>(((driver_index % drivers) + drivers) % drivers);
    const auto p = static_cast<uint32_t>(((passenger_index % passengers) + passengers) % passengers);
    const uint32_t key = d | p << 4 | static_cast<uint32_t>(steer + 1) << 8 | static_cast<uint32_t>(wave + 1) << 10 |
                         static_cast<uint32_t>(frame & 1) << 12 | static_cast<uint32_t>(convertible) << 13 |
                         static_cast<uint32_t>(bandaged) << 14;
    auto it = occupants_.find(key);
    if (it == occupants_.end()) {
        it = occupants_.emplace(key, make_occupants(driver(static_cast<int>(d)), passenger(static_cast<int>(p)), steer,
                                                    wave, frame, convertible, player_headroom, bandaged)).first;
    }
    return it->second;
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

    // The player's cars, one per CarModel: its colours, and a sign on the
    // roof for the taxi (else it would be the yellow Hot Hatch).
    struct PlayerLook {
        CarStyle style;
        bool taxi_sign;
    };
    const PlayerLook looks[car_models] = {
        {{{0x88, 0x08, 0x10}, {0xd0, 0x18, 0x1c}, {0xf0, 0x60, 0x50}, true}, false},  // Spider
        {{{0x10, 0x1c, 0x48}, {0x1c, 0x34, 0x7c}, {0x50, 0x78, 0xc0}, false}, false}, // GT Coupe
        {{{0xb0, 0x88, 0x08}, {0xf0, 0xc8, 0x20}, {0xff, 0xec, 0x80}, false}, false}, // Hot Hatch
        {{{0x0c, 0x0c, 0x10}, {0x24, 0x24, 0x2a}, {0xe0, 0xb0, 0x30}, true}, false},  // Muscle
        {{{0xa0, 0x40, 0x08}, {0xe8, 0x78, 0x18}, {0xff, 0xb0, 0x58}, false}, false}, // Big Rig
        {{{0x14, 0x2c, 0x80}, {0x24, 0x50, 0xc8}, {0x70, 0x98, 0xf0}, false}, false}, // Saloon
        {{{0xa0, 0x80, 0x10}, {0xe8, 0xc0, 0x20}, {0xf8, 0xe8, 0x80}, false}, true},  // Taxi
        {{{0x14, 0x5c, 0x30}, {0x24, 0x8c, 0x48}, {0x70, 0xc8, 0x88}, false}, false}, // Estate
        {{{0x10, 0x10, 0x14}, {0xf0, 0xf0, 0xf4}, {0xff, 0xff, 0xff}, false}, false}, // Patrol
        {{{0x0c, 0x0c, 0x10}, {0x20, 0x20, 0x26}, {0xf0, 0xc0, 0x30}, false}, false}, // Racer
        {{{0xb8, 0xb8, 0xc0}, {0xf0, 0xf0, 0xf4}, {0xff, 0xff, 0xff}, false}, false}, // Van
        {{{0x8c, 0x18, 0x18}, {0xc0, 0x28, 0x24}, {0xe8, 0x60, 0x50}, false}, false}, // Box truck
    };
    for (int model = 0; model < car_models; ++model) {
        const PlayerLook& look = looks[model];
        const CarStyle& st = look.style;
        const Body body = car_model(model).body;
        player_convertible_[static_cast<size_t>(model)] = body == Body::Car && st.convertible;
        dashboards_[static_cast<size_t>(model)] = make_dashboard(st, 320);
        for (int turn = -1; turn <= 1; ++turn) {
            for (int brake = 0; brake < 2; ++brake) {
                for (int signal = -1; signal <= hazard_signal; ++signal) {
                    for (int t = 0; t < tyre_frames; ++t) {
                        Bitmap b;
                        switch (body) {
                            case Body::Rig: b = make_player_truck(st, turn, brake, signal, t, player_headroom); break;
                            case Body::Van: b = make_van(st, signal, brake, t); break;
                            case Body::BoxTruck: b = make_truck(st, signal, brake, t); break;
                            case Body::Racer: b = make_rival(st, signal, brake, t); break;
                            case Body::Police: b = make_player_police(st, turn, brake, signal, t, player_headroom); break;
                            default: b = make_player_car(st, turn, brake, signal, t, player_headroom); break;
                        }
                        if (look.taxi_sign) taxi_sign(b, 2 * turn, player_headroom);
                        player_[static_cast<size_t>(model)][static_cast<size_t>(turn + 1)][static_cast<size_t>(brake)]
                               [static_cast<size_t>(signal + 1)][static_cast<size_t>(t)] = std::move(b);
                    }
                }
            }
        }
    }

    for (int d = 0; d < drivers; ++d) wheels_[static_cast<size_t>(d)] = make_wheel(driver(d));

    // Colour schemes per vehicle kind: dark, body, light (stripes on the rival).
    const std::vector<CarStyle> styles[] = {
        /* Car */ {{{0x14, 0x2c, 0x80}, {0x24, 0x50, 0xc8}, {0x70, 0x98, 0xf0}, false},
                   {{0xa0, 0x80, 0x10}, {0xe8, 0xc0, 0x20}, {0xf8, 0xe8, 0x80}, false},
                   {{0x98, 0x98, 0xa0}, {0xd8, 0xd8, 0xe0}, {0xf8, 0xf8, 0xff}, false},
                   {{0x14, 0x5c, 0x30}, {0x24, 0x8c, 0x48}, {0x70, 0xc8, 0x88}, false}},
        /* Van */ {{{0xb8, 0xb8, 0xc0}, {0xf0, 0xf0, 0xf4}, {0xff, 0xff, 0xff}, false},
                   {{0x7c, 0x50, 0x14}, {0xb8, 0x7c, 0x24}, {0xe0, 0xa8, 0x50}, false},
                   {{0x1c, 0x4c, 0x6c}, {0x2c, 0x74, 0x9c}, {0x68, 0xa8, 0xcc}, false}},
        /* Truck */ {{{0xb0, 0xb0, 0xb8}, {0xdc, 0xdc, 0xe0}, {0xf4, 0xf4, 0xf8}, false},
                     {{0x8c, 0x18, 0x18}, {0xc0, 0x28, 0x24}, {0xe8, 0x60, 0x50}, false},
                     {{0x20, 0x50, 0x28}, {0x30, 0x78, 0x3c}, {0x68, 0xb0, 0x70}, false}},
        /* Rival */ {{{0x0c, 0x0c, 0x10}, {0x20, 0x20, 0x26}, {0xf0, 0xc0, 0x30}, false},
                     {{0x3c, 0x10, 0x58}, {0x64, 0x20, 0x8c}, {0xf0, 0xf0, 0xf8}, false}},
        /* Police */ {{{0x10, 0x10, 0x14}, {0xf0, 0xf0, 0xf4}, {0xff, 0xff, 0xff}, false}},
    };
    static_assert(sizeof(styles) / sizeof(styles[0]) == static_cast<size_t>(Vehicle::Count),
                  "every vehicle kind needs its colours");
    for (int k = 0; k < static_cast<int>(Vehicle::Count); ++k) {
        const auto kind = static_cast<Vehicle>(k);
        for (const CarStyle& st : styles[k]) {
            VehicleSprites v;
            for (int signal = -1; signal <= 1; ++signal) {
                const auto s = static_cast<size_t>(signal + 1);
                for (int t = 0; t < tyre_frames; ++t) {
                    const auto tf = static_cast<size_t>(t);
                    for (int brake = 0; brake < 2; ++brake) {
                        Bitmap& rear = v.rear[s][static_cast<size_t>(brake)][tf];
                        switch (kind) {
                            case Vehicle::Van: rear = make_van(st, signal, brake, t); break;
                            case Vehicle::Truck: rear = make_truck(st, signal, brake, t); break;
                            case Vehicle::Rival: rear = make_rival(st, signal, brake, t); break;
                            case Vehicle::Police: rear = make_police(st, signal, brake, t); break;
                            default: rear = make_car(st, 0, signal, brake, t); break;
                        }
                    }
                    switch (kind) {
                        case Vehicle::Van: v.front[s][tf] = make_van_front(st, signal, t); break;
                        case Vehicle::Truck: v.front[s][tf] = make_truck_front(st, signal, t); break;
                        case Vehicle::Rival: v.front[s][tf] = make_rival_front(st, signal, t); break;
                        case Vehicle::Police: v.front[s][tf] = make_police_front(st, signal, t); break;
                        default: v.front[s][tf] = make_car_front(st, signal, t); break;
                    }
                }
            }
            vehicles_[static_cast<size_t>(k)].push_back(std::move(v));
        }
    }
    billboard_back_ = make_billboard_back();
    scenery_[static_cast<size_t>(Scenery::GasStation)] = make_gas_station();
    scenery_[static_cast<size_t>(Scenery::FuelPump)] = make_fuel_pump();
    scenery_[static_cast<size_t>(Scenery::FuelSign)] = make_fuel_sign();
    scenery_[static_cast<size_t>(Scenery::Victorian)] = make_victorian(Color{0x6c, 0x9c, 0xc8}, Color{0xc8, 0x50, 0x40});
    scenery_[static_cast<size_t>(Scenery::VictorianB)] = make_victorian(Color{0xe4, 0xc4, 0x6c}, Color{0x3c, 0x6c, 0x5c});
    scenery_[static_cast<size_t>(Scenery::VictorianC)] = make_victorian(Color{0xd4, 0x8c, 0xa8}, Color{0x5c, 0x3c, 0x7c});
    scenery_[static_cast<size_t>(Scenery::StreetLamp)] = make_street_lamp();
    scenery_[static_cast<size_t>(Scenery::SignLeft)] = make_fork_sign(-1);
    scenery_[static_cast<size_t>(Scenery::SignRight)] = make_fork_sign(1);
    scenery_[static_cast<size_t>(Scenery::Hedge)] = make_hedge();
    scenery_[static_cast<size_t>(Scenery::StoneWall)] = make_stone_wall();
    scenery_[static_cast<size_t>(Scenery::PhoneBox)] = make_phone_box();
    scenery_[static_cast<size_t>(Scenery::Windmill)] = make_windmill();
    scenery_[static_cast<size_t>(Scenery::Tulips)] = make_tulips();
    scenery_[static_cast<size_t>(Scenery::CherryTree)] = make_cherry_tree();
    scenery_[static_cast<size_t>(Scenery::Torii)] = make_torii();
    scenery_[static_cast<size_t>(Scenery::StoneLantern)] = make_stone_lantern();
    // The everyday cars' dealer in blue, the sports cars' in red.
    const ShowroomLook regular{"CARS", {0x1c, 0x4c, 0xa8}, {0x2c, 0x60, 0xc8},
                               {{{0xf0, 0xc8, 0x20}, {0xff, 0xec, 0x80}}, {{0x24, 0x50, 0xc8}, {0x70, 0x98, 0xf0}},
                                {{0x24, 0x8c, 0x48}, {0x70, 0xc8, 0x88}}, {{0xe8, 0xc0, 0x20}, {0xf8, 0xe8, 0x80}}}};
    const ShowroomLook sports{"SPORTS", {0xa8, 0x14, 0x20}, {0xd0, 0x28, 0x30},
                              {{{0xd0, 0x18, 0x1c}, {0xf0, 0x60, 0x50}}, {{0x1c, 0x34, 0x7c}, {0x50, 0x78, 0xc0}},
                               {{0x24, 0x24, 0x2a}, {0xe0, 0xb0, 0x30}}, {{0x20, 0x20, 0x26}, {0xf0, 0xc0, 0x30}}}};
    scenery_[static_cast<size_t>(Scenery::Showroom)] = make_showroom(regular);
    scenery_[static_cast<size_t>(Scenery::DealerSign)] = make_dealer_sign(regular);
    scenery_[static_cast<size_t>(Scenery::SportsShowroom)] = make_showroom(sports);
    scenery_[static_cast<size_t>(Scenery::SportsSign)] = make_dealer_sign(sports);
    scenery_[static_cast<size_t>(Scenery::DatePalm)] = make_date_palm();
    scenery_[static_cast<size_t>(Scenery::Pyramid)] = make_pyramid();
    scenery_[static_cast<size_t>(Scenery::Acacia)] = make_acacia();
    scenery_[static_cast<size_t>(Scenery::Giraffe)] = make_giraffe();
    scenery_[static_cast<size_t>(Scenery::TermiteMound)] = make_termite_mound();
    scenery_[static_cast<size_t>(Scenery::Banyan)] = make_banyan();
    scenery_[static_cast<size_t>(Scenery::Temple)] = make_temple();
    scenery_[static_cast<size_t>(Scenery::Cow)] = make_cow();
    scenery_[static_cast<size_t>(Scenery::Maple)] = make_maple();
    scenery_[static_cast<size_t>(Scenery::HanokGate)] = make_hanok_gate();
    scenery_[static_cast<size_t>(Scenery::GumTree)] = make_gum_tree();
    scenery_[static_cast<size_t>(Scenery::KangarooSign)] = make_kangaroo_sign();
    scenery_[static_cast<size_t>(Scenery::Uluru)] = make_uluru();
    scenery_[static_cast<size_t>(Scenery::JungleTree)] = make_jungle_tree();
    scenery_[static_cast<size_t>(Scenery::Banana)] = make_banana();
    scenery_[static_cast<size_t>(Scenery::CarWash)] = make_car_wash();
    scenery_[static_cast<size_t>(Scenery::WashSign)] = make_wash_sign();
    scenery_[static_cast<size_t>(Scenery::Motel)] = make_motel();
    scenery_[static_cast<size_t>(Scenery::MotelSign)] = make_motel_sign();
    scenery_[static_cast<size_t>(Scenery::Hospital)] = make_hospital();
    scenery_[static_cast<size_t>(Scenery::HospitalSign)] = make_hospital_sign();
    scenery_[static_cast<size_t>(Scenery::Truckstop)] = make_truckstop();
    scenery_[static_cast<size_t>(Scenery::TruckSign)] = make_truck_sign();
}

namespace {

// Smooth noise 0 .. 1 over the plane, varying over about `cell` pixels: the
// same at the same place every time.
float value_noise(int x, int y, int cell, uint32_t seed) {
    const auto corner = [seed](int cx, int cy) {
        uint32_t h = static_cast<uint32_t>(cx) * 73856093u ^ static_cast<uint32_t>(cy) * 19349663u ^ seed;
        h = (h ^ (h >> 13)) * 0x5bd1e995u;
        return static_cast<float>((h >> 8) & 0xffff) / 65536.f;
    };
    const int cx = x / cell, cy = y / cell;
    const float fx = static_cast<float>(x % cell) / static_cast<float>(cell);
    const float fy = static_cast<float>(y % cell) / static_cast<float>(cell);
    const float top = corner(cx, cy) + (corner(cx + 1, cy) - corner(cx, cy)) * fx;
    const float bottom = corner(cx, cy + 1) + (corner(cx + 1, cy + 1) - corner(cx, cy + 1)) * fx;
    return top + (bottom - top) * fy;
}

} // namespace

void apply_dirt(Bitmap& car, float mud, float oil) {
    if (mud <= 0.f && oil <= 0.f) return;
    const Color mud_color{0x6c, 0x52, 0x34}, oil_color{0x16, 0x14, 0x12};
    const uint32_t outline = Outline.argb();
    for (int y = 0; y < car.h; ++y) {
        // Mud is thrown up from below: it reaches the lower body first.
        const float low = static_cast<float>(y) / static_cast<float>(car.h);
        for (int x = 0; x < car.w; ++x) {
            uint32_t& p = car.px[static_cast<size_t>(y) * car.w + x];
            if (!(p >> 24) || p == outline) continue;
            const Color c{static_cast<uint8_t>(p >> 16), static_cast<uint8_t>(p >> 8), static_cast<uint8_t>(p)};
            // Splotches: smooth noise above a threshold that falls with the
            // dirt, plus a fine grain at their edges.
            const float grain = static_cast<float>((static_cast<uint32_t>(x * 7 + y * 13) * 2654435761u) >> 28) / 64.f;
            const float m = value_noise(x, y, 5, 0x1234u) + 0.25f * value_noise(x, y, 2, 0x99u) + grain;
            const float o = value_noise(x, y, 3, 0xbeefu) + grain;
            if (o > 1.25f - 0.45f * oil) {
                p = blend(c, oil_color, 0.8f).argb();
            } else if (m > 1.45f - mud * (0.6f + 0.9f * low)) {
                p = blend(c, mud_color, 0.35f + 0.35f * mud).argb();
            }
        }
    }
}

} // namespace racer
