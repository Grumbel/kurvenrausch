// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "overlay.hpp"

#include "font.hpp"

#include <algorithm>
#include <cmath>

namespace racer {

namespace {

uint8_t byte(float v) {
    return static_cast<uint8_t>(std::clamp(std::lround(v * 255.f), 0L, 255L));
}

} // namespace

template <typename Make>
void Overlay::place(std::string key, int x, int y, Make&& make) {
    auto it = images_.find(key);
    if (it == images_.end()) it = images_.emplace(std::move(key), make()).first;
    items_.push_back(Item{&it->first, &it->second, x, y});
}

void Overlay::disc(float cx, float cy, float r, float rim, bool pressed) {
    const int n = 2 * static_cast<int>(std::ceil(r)) + 2;
    const float c = static_cast<float>(n) / 2.f;
    const std::string key = "disc " + std::to_string(std::lround(r * 4.f)) + " " +
                            std::to_string(std::lround(rim * 4.f)) + (pressed ? " on" : "");
    place(key, static_cast<int>(std::lround(cx - c)), static_cast<int>(std::lround(cy - c)), [&] {
        const Color fill = pressed ? Color{0xff, 0xd8, 0x30} : Color{0x20, 0x20, 0x30};
        const float fill_alpha = pressed ? 0.45f : 0.35f;
        const Color edge{0xf8, 0xf8, 0xf8};
        constexpr float edge_alpha = 0.6f;
        Bitmap b(n, n);
        for (int y = 0; y < n; ++y) {
            for (int x = 0; x < n; ++x) {
                const float dx = static_cast<float>(x) + 0.5f - c, dy = static_cast<float>(y) + 0.5f - c;
                const float d = std::sqrt(dx * dx + dy * dy);
                // Soft edges a pixel wide: at screen resolution the steps
                // would show.
                const float cover = std::clamp(r - d + 0.5f, 0.f, 1.f);
                if (cover <= 0.f) continue;
                const float t = std::clamp(d - (r - rim) + 0.5f, 0.f, 1.f);
                const Color mix = blend(fill, edge, t);
                const float alpha = (fill_alpha + (edge_alpha - fill_alpha) * t) * cover;
                b.set(x, y, Color{mix.r, mix.g, mix.b, byte(alpha)});
            }
        }
        return b;
    });
}

void Overlay::text(float cx, float cy, std::string_view s, int scale) {
    const int w = font::text_width(s, scale);
    const std::string key = "text " + std::to_string(scale) + " " + std::string(s);
    const int x = static_cast<int>(std::lround(cx - static_cast<float>(w) / 2.f));
    const int y = static_cast<int>(std::lround(cy - static_cast<float>(font::glyph_h * scale) / 2.f));
    place(key, x, y, [&] {
        Bitmap b(w + scale, (font::glyph_h + 1) * scale);
        font::render(scale, scale, s, scale, [&](int px, int py) { b.set(px, py, Color{0x10, 0x10, 0x20}); });
        font::render(0, 0, s, scale, [&](int px, int py) { b.set(px, py, Color{0xf8, 0xf8, 0xf8}); });
        return b;
    });
}

void Overlay::rect(float x, float y, float w, float h, Color c) {
    const int iw = std::max(1, static_cast<int>(std::lround(w)));
    const int ih = std::max(1, static_cast<int>(std::lround(h)));
    const std::string key = "rect " + std::to_string(iw) + "x" + std::to_string(ih) + " " + std::to_string(c.argb());
    place(key, static_cast<int>(std::lround(x)), static_cast<int>(std::lround(y)), [&] {
        Bitmap b(iw, ih);
        std::fill(b.px.begin(), b.px.end(), c.argb());
        return b;
    });
}

void Overlay::draw(Canvas& fb) const {
    for (const Item& item : items_) {
        const Bitmap& b = *item.image;
        for (int y = 0; y < b.h; ++y) {
            for (int x = 0; x < b.w; ++x) {
                const uint32_t p = b.get(x, y);
                if (p >> 24 == 0) continue;
                fb.blend_pixel(item.x + x, item.y + y,
                               Color{static_cast<uint8_t>(p >> 16), static_cast<uint8_t>(p >> 8), static_cast<uint8_t>(p)},
                               static_cast<float>(p >> 24) / 255.f);
            }
        }
    }
}

} // namespace racer
