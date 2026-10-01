#include "sprites.hpp"

#include "font.hpp"

#include <cmath>

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

Bitmap make_boulder() {
    Bitmap b(48, 32);
    const Color dark{0x58, 0x50, 0x48}, mid{0x8a, 0x82, 0x74}, light{0xb6, 0xae, 0x9c};
    paint::shaded_ellipse(b, 22.f, 19.f, 20.f, 13.f, dark, mid, light);
    paint::shaded_ellipse(b, 37.f, 24.f, 10.f, 8.f, dark, mid, light);
    paint::stroke(b, 18.f, 10.f, 23.f, 18.f, 1.f, 1.f, dark);
    paint::stroke(b, 23.f, 18.f, 20.f, 25.f, 1.f, 1.f, dark);
    paint::outline(b, Outline);
    return b;
}

Bitmap make_billboard() {
    Bitmap b(96, 64);
    const Color post{0x6a, 0x6a, 0x70}, post_dark{0x44, 0x44, 0x4a};
    paint::rect(b, 14, 40, 6, 24, post);
    paint::rect(b, 18, 40, 2, 24, post_dark);
    paint::rect(b, 76, 40, 6, 24, post);
    paint::rect(b, 80, 40, 2, 24, post_dark);

    paint::rect(b, 0, 0, 96, 42, Color{0xf0, 0xf0, 0xf0});
    for (int y = 3; y < 39; ++y) {
        const float t = static_cast<float>(y - 3) / 36.f;
        paint::rect(b, 3, y, 90, 1, blend(Color{0x18, 0x30, 0xa0}, Color{0x70, 0x18, 0x90}, t));
    }
    const Color ink{0xff, 0xd8, 0x20}, shadow{0x10, 0x08, 0x20};
    const int x = (96 - font::text_width("KURVEN", 2)) / 2;
    paint::text(b, x + 1, 7, "KURVEN", shadow, 2);
    paint::text(b, x, 6, "KURVEN", ink, 2);
    paint::text(b, x + 1, 23, "RAUSCH", shadow, 2);
    paint::text(b, x, 22, "RAUSCH", ink, 2);

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
    scenery_[static_cast<size_t>(Scenery::Boulder)] = make_boulder();
    scenery_[static_cast<size_t>(Scenery::Billboard)] = make_billboard();
    scenery_[static_cast<size_t>(Scenery::Gantry)] = make_gantry();

    const CarStyle player{{0x88, 0x08, 0x10}, {0xd0, 0x18, 0x1c}, {0xf0, 0x60, 0x50}, true};
    for (int turn = -1; turn <= 1; ++turn) {
        player_[static_cast<size_t>(turn + 1)] = make_car(player, turn);
    }
}

} // namespace racer
