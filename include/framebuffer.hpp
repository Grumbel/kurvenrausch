// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include "bitmap.hpp"
#include "types.hpp"

#include <cstdint>
#include <string_view>
#include <vector>

namespace racer {

// Software ARGB8888 framebuffer with a clip rectangle and the few primitives
// a classic scanline racer needs. All drawing respects the clip rectangle.
class Framebuffer {
public:
    Framebuffer(int width, int height);

    int width() const { return w_; }
    int height() const { return h_; }
    const uint32_t* pixels() const { return pixels_.data(); }
    uint32_t* pixels_mut() { return pixels_.data(); }

    // Clip rectangle, half-open: [x0, x1) x [y0, y1).
    void set_clip(int x0, int y0, int x1, int y1);
    void reset_clip();
    int clip_bottom() const { return clip_y1_; }

    void clear(Color c);
    void put_pixel(int x, int y, Color c);
    // Blends c over the existing pixel; alpha 0 leaves it, 1 replaces it.
    void blend_pixel(int x, int y, Color c, float alpha);
    // Horizontal line from x0 (inclusive) to x1 (exclusive).
    void hline(int x0, int x1, int y, Color c);
    void fill_rect(int x, int y, int w, int h, Color c);
    // Line from (x0, y0) to (x1, y1), both ends included.
    void line(int x0, int y0, int x1, int y1, Color c);

    // Fills a trapezoid with horizontal top and bottom edges. Coordinates are
    // sub-pixel; a pixel is covered if its centre lies inside, so adjacent
    // trapezoids sharing an edge neither overlap nor leave gaps.
    void fill_trapezoid(float y_top, float xl_top, float xr_top,
                        float y_bot, float xl_bot, float xr_bot, Color c);

    // Draws a bitmap scaled to the given sub-pixel rectangle, skipping
    // transparent pixels and blending the rest towards `fog` by fog_amount.
    void blit_scaled(const Bitmap& bmp, float x, float y, float w, float h,
                     bool flip = false, float fog_amount = 0.f, Color fog = Color{});

    // Draws a bitmap scaled to w x h and rotated by `angle` radians (clockwise)
    // about its centre (cx, cy), skipping transparent pixels.
    void blit_rotated(const Bitmap& bmp, float cx, float cy, float w, float h, float angle);

    // Copies another framebuffer 1:1 with its top-left corner at (x, y).
    void blit(const Framebuffer& src, int x, int y);

    // Text in the 5x7 bitmap font, top-left at (x, y).
    void draw_text(int x, int y, std::string_view s, Color c, int scale = 1);

private:
    int w_, h_;
    std::vector<int> columns_; // scratch: source column per destination column
    int clip_x0_ = 0, clip_y0_ = 0, clip_x1_ = 0, clip_y1_ = 0;
    std::vector<uint32_t> pixels_;
};

// Converts a sub-pixel edge coordinate to the first pixel whose centre lies at
// or beyond it. Clamped so wild projections can't overflow the int conversion.
int pixel_edge(float v);


} // namespace racer
