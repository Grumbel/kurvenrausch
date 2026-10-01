// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include "types.hpp"

#include <cstdint>
#include <string_view>
#include <vector>

namespace racer {

// ARGB8888 image; alpha 0 marks transparent pixels.
struct Bitmap {
    int w = 0;
    int h = 0;
    std::vector<uint32_t> px;

    Bitmap() = default;
    Bitmap(int width, int height)
        : w(width), h(height), px(static_cast<size_t>(width) * static_cast<size_t>(height), 0u) {}

    bool inside(int x, int y) const { return x >= 0 && y >= 0 && x < w && y < h; }
    uint32_t get(int x, int y) const { return px[static_cast<size_t>(y) * w + x]; }
    bool opaque(int x, int y) const { return inside(x, y) && (get(x, y) >> 24) != 0; }
    void set(int x, int y, Color c) {
        if (inside(x, y)) px[static_cast<size_t>(y) * w + x] = c.argb();
    }
};

// Drawing helpers for generating pixel art.
namespace paint {

void rect(Bitmap& b, int x, int y, int w, int h, Color c);
void ellipse(Bitmap& b, float cx, float cy, float rx, float ry, Color c);
// Ellipse lit from the upper left, banded into three tones with ordered
// dithering between bands, the classic hand-pixelled look.
void shaded_ellipse(Bitmap& b, float cx, float cy, float rx, float ry,
                    Color dark, Color mid, Color light);
// Line with thickness tapering linearly from t0 to t1.
void stroke(Bitmap& b, float x0, float y0, float x1, float y1, float t0, float t1, Color c);
// Adds a one pixel outline around all opaque pixels.
void outline(Bitmap& b, Color c);
// Text in the 5x7 bitmap font.
void text(Bitmap& b, int x, int y, std::string_view s, Color c, int scale = 1);

} // namespace paint

} // namespace racer
