// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "sprites.hpp"

#include "font.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
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

Bitmap make_car(const CarStyle& style, int turn, int signal, bool brake, int tread_frame) {
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
        const bool on = signal == side;
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
        if (signal == side) {
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

Bitmap make_player_car(const CarStyle& style, int turn, int side, int frame, bool brake, int tread, int headroom) {
    const Bitmap car = make_car(style, turn, 0, brake, tread);
    Bitmap b(car.w, car.h + headroom);
    std::copy(car.px.begin(), car.px.end(), b.px.begin() + static_cast<std::ptrdiff_t>(headroom) * car.w);
    if (side == 0) return b;

    // A raised arm and open hand, waving: outlined on its own, then laid over
    // the car so the car's outline does not run through it.
    const Color skin{0xf0, 0xbc, 0x8c}, skin_dark{0xc4, 0x88, 0x5c};
    const float u = static_cast<float>(2 * turn); // the cabin leans like in make_car
    const float h = static_cast<float>(headroom);
    const float sx = (side < 0 ? 30.f : 66.f) + u, sy = h + 11.f;   // shoulder
    const float out = static_cast<float>(side) * (frame ? 4.f : 8.f); // lean of the wave
    const float hx = sx + out, hy = h - 7.f + (frame ? 0.f : 1.f);   // hand
    Bitmap arm(b.w, b.h);
    paint::stroke(arm, sx, sy, hx, hy + 3.f, 2.6f, 2.f, skin);
    paint::stroke(arm, sx + 1.f, sy, hx + 1.f, hy + 3.f, 1.f, 1.f, skin_dark);
    paint::ellipse(arm, hx, hy, 2.5f, 3.f, skin);
    paint::rect(arm, static_cast<int>(hx) - (side < 0 ? 4 : -3), static_cast<int>(hy), 2, 1, skin); // thumb
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
        indicator(b, x, 25, 6, 6, signal == side);
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
    for (int side = -1; side <= 1; side += 2) indicator(b, side < 0 ? 8 : 90, 43, 6, 3, signal == side);
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
        indicator(b, side < 0 ? x : x + 12, 84, 6, 4, signal == side);
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
    for (int side = -1; side <= 1; side += 2) indicator(b, side < 0 ? 12 : 94, 58, 6, 5, signal == side);
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
    indicator(b, 7, 19, 3, 4, signal < 0);
    indicator(b, 90, 19, 3, 4, signal > 0);
    paint::rect(b, 20, 30, 60, 5, Color{0x14, 0x14, 0x18}); // diffuser
    for (int x = 24; x < 78; x += 6) paint::rect(b, x, 30, 1, 5, Color{0x40, 0x40, 0x48});
    for (float x : {44.f, 56.f}) paint::ellipse(b, x, 31.f, 2.5f, 2.f, Color{0xb0, 0xb0, 0xbc});
    paint::rect(b, 42, 25, 16, 4, Color{0xe8, 0xe8, 0xd8});
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
    indicator(b, 8, 23, 6, 2, signal < 0);
    indicator(b, 86, 23, 6, 2, signal > 0);
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
        auto& poses = player_[static_cast<size_t>(turn + 1)];
        for (int brake = 0; brake < 2; ++brake) {
            const auto b = static_cast<size_t>(brake);
            for (int t = 0; t < tyre_frames; ++t) {
                const auto tf = static_cast<size_t>(t);
                poses[0][b][tf] = make_player_car(player, turn, 0, 0, brake, t, player_headroom);
                for (int frame = 0; frame < 2; ++frame) {
                    poses[static_cast<size_t>(1 + frame)][b][tf] = make_player_car(player, turn, -1, frame, brake, t, player_headroom);
                    poses[static_cast<size_t>(3 + frame)][b][tf] = make_player_car(player, turn, 1, frame, brake, t, player_headroom);
                }
            }
        }
    }

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
                            default: rear = make_car(st, 0, signal, brake, t); break;
                        }
                    }
                    switch (kind) {
                        case Vehicle::Van: v.front[s][tf] = make_van_front(st, signal, t); break;
                        case Vehicle::Truck: v.front[s][tf] = make_truck_front(st, signal, t); break;
                        case Vehicle::Rival: v.front[s][tf] = make_rival_front(st, signal, t); break;
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
}

} // namespace racer
