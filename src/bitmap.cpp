// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "bitmap.hpp"

#include "font.hpp"

#include <algorithm>
#include <cmath>

namespace racer::paint {

void rect(Bitmap& b, int x, int y, int w, int h, Color c) {
    for (int j = y; j < y + h; ++j)
        for (int i = x; i < x + w; ++i) b.set(i, j, c);
}

void ellipse(Bitmap& b, float cx, float cy, float rx, float ry, Color c) {
    if (rx <= 0.f || ry <= 0.f) return;
    const int y0 = static_cast<int>(std::floor(cy - ry));
    const int y1 = static_cast<int>(std::ceil(cy + ry));
    const int x0 = static_cast<int>(std::floor(cx - rx));
    const int x1 = static_cast<int>(std::ceil(cx + rx));
    for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1; ++x) {
            const float nx = (static_cast<float>(x) + 0.5f - cx) / rx;
            const float ny = (static_cast<float>(y) + 0.5f - cy) / ry;
            if (nx * nx + ny * ny <= 1.f) b.set(x, y, c);
        }
    }
}

void shaded_ellipse(Bitmap& b, float cx, float cy, float rx, float ry,
                    Color dark, Color mid, Color light) {
    if (rx <= 0.f || ry <= 0.f) return;
    // Light from the upper left, slightly towards the viewer.
    const float lx = -0.55f, ly = -0.6f, lz = 0.58f;
    const int y0 = static_cast<int>(std::floor(cy - ry));
    const int y1 = static_cast<int>(std::ceil(cy + ry));
    const int x0 = static_cast<int>(std::floor(cx - rx));
    const int x1 = static_cast<int>(std::ceil(cx + rx));
    for (int y = y0; y <= y1; ++y) {
        for (int x = x0; x <= x1; ++x) {
            const float nx = (static_cast<float>(x) + 0.5f - cx) / rx;
            const float ny = (static_cast<float>(y) + 0.5f - cy) / ry;
            const float r2 = nx * nx + ny * ny;
            if (r2 > 1.f) continue;
            const float nz = std::sqrt(1.f - r2);
            const float d = nx * lx + ny * ly + nz * lz + (bayer4(x, y) - 0.5f) * 0.25f;
            b.set(x, y, d > 0.72f ? light : d > 0.3f ? mid : dark);
        }
    }
}

void stroke(Bitmap& b, float x0, float y0, float x1, float y1, float t0, float t1, Color c) {
    const float len = std::hypot(x1 - x0, y1 - y0);
    const int steps = std::max(1, static_cast<int>(len * 2.f));
    for (int i = 0; i <= steps; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(steps);
        const float r = (t0 + (t1 - t0) * t) / 2.f;
        const float x = x0 + (x1 - x0) * t;
        const float y = y0 + (y1 - y0) * t;
        if (r < 0.75f) b.set(static_cast<int>(std::floor(x)), static_cast<int>(std::floor(y)), c);
        else ellipse(b, x, y, r, r, c);
    }
}

void outline(Bitmap& b, Color c) {
    const Bitmap src = b;
    for (int y = 0; y < b.h; ++y) {
        for (int x = 0; x < b.w; ++x) {
            if (src.opaque(x, y)) continue;
            if (src.opaque(x - 1, y) || src.opaque(x + 1, y) ||
                src.opaque(x, y - 1) || src.opaque(x, y + 1)) {
                b.set(x, y, c);
            }
        }
    }
}

void text(Bitmap& b, int x, int y, std::string_view s, Color c, int scale) {
    font::render(x, y, s, scale, [&](int px, int py) { b.set(px, py, c); });
}

} // namespace racer::paint
