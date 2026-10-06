// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "framebuffer.hpp"

#include "font.hpp"

#include <algorithm>
#include <cmath>

namespace racer {

int pixel_edge(float v) {
    constexpr float limit = 1.0e6f;
    return static_cast<int>(std::ceil(std::clamp(v, -limit, limit) - 0.5f));
}

Framebuffer::Framebuffer(int width, int height)
    : w_(width), h_(height),
      pixels_(static_cast<size_t>(width) * static_cast<size_t>(height), 0u) {
    reset_clip();
}

void Framebuffer::set_clip(int x0, int y0, int x1, int y1) {
    clip_x0_ = std::clamp(x0, 0, w_);
    clip_y0_ = std::clamp(y0, 0, h_);
    clip_x1_ = std::clamp(x1, clip_x0_, w_);
    clip_y1_ = std::clamp(y1, clip_y0_, h_);
}

void Framebuffer::reset_clip() {
    set_clip(0, 0, w_, h_);
}

void Framebuffer::clear(Color c) {
    std::fill(pixels_.begin(), pixels_.end(), c.argb());
}

void Framebuffer::put_pixel(int x, int y, Color c) {
    if (x < clip_x0_ || x >= clip_x1_ || y < clip_y0_ || y >= clip_y1_) return;
    pixels_[static_cast<size_t>(y) * w_ + x] = c.argb();
}

void Framebuffer::blend_pixel(int x, int y, Color c, float alpha) {
    if (x < clip_x0_ || x >= clip_x1_ || y < clip_y0_ || y >= clip_y1_) return;
    uint32_t& dst = pixels_[static_cast<size_t>(y) * w_ + x];
    const float src_a = std::clamp(alpha, 0.f, 1.f);
    const float dst_a = static_cast<float>((dst >> 24) & 0xffu) / 255.f;
    const float out_a = src_a + dst_a * (1.f - src_a);
    if (out_a < 1e-4f) {
        dst = 0;
        return;
    }
    auto ch = [&](uint8_t s, uint8_t d) {
        const float s_f = static_cast<float>(s);
        const float d_f = static_cast<float>(d);
        // src-over in straight alpha; matches the opaque case used on the software path.
        return static_cast<uint8_t>((s_f * src_a + d_f * dst_a * (1.f - src_a)) / out_a + 0.5f);
    };
    const uint8_t dr = static_cast<uint8_t>((dst >> 16) & 0xffu);
    const uint8_t dg = static_cast<uint8_t>((dst >> 8) & 0xffu);
    const uint8_t db = static_cast<uint8_t>(dst & 0xffu);
    dst = Color(ch(c.r, dr), ch(c.g, dg), ch(c.b, db),
                static_cast<uint8_t>(out_a * 255.f + 0.5f))
              .argb();
}

void Framebuffer::hline(int x0, int x1, int y, Color c) {
    if (y < clip_y0_ || y >= clip_y1_) return;
    x0 = std::max(x0, clip_x0_);
    x1 = std::min(x1, clip_x1_);
    if (x0 >= x1) return;
    uint32_t* row = &pixels_[static_cast<size_t>(y) * w_];
    std::fill(row + x0, row + x1, c.argb());
}

void Framebuffer::fill_rect(int x, int y, int w, int h, Color c) {
    for (int j = 0; j < h; ++j) hline(x, x + w, y + j, c);
}

void Framebuffer::blit_rotated(const Bitmap& bmp, float cx, float cy, float w, float h, float angle) {
    if (bmp.w <= 0 || bmp.h <= 0 || !(w > 0.f) || !(h > 0.f)) return;
    const float c = std::cos(angle), s = std::sin(angle);
    const float r = 0.5f * std::hypot(w, h) + 1.f;
    const int x0 = std::max(clip_x0_, pixel_edge(cx - r)), x1 = std::min(clip_x1_, pixel_edge(cx + r));
    const int y0 = std::max(clip_y0_, pixel_edge(cy - r)), y1 = std::min(clip_y1_, pixel_edge(cy + r));
    const float sx = static_cast<float>(bmp.w) / w, sy = static_cast<float>(bmp.h) / h;
    for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) {
            // Rotate the pixel centre back into the bitmap's frame.
            const float dx = static_cast<float>(x) + 0.5f - cx, dy = static_cast<float>(y) + 0.5f - cy;
            const float u = (c * dx + s * dy) * sx + static_cast<float>(bmp.w) / 2.f;
            const float v = (-s * dx + c * dy) * sy + static_cast<float>(bmp.h) / 2.f;
            if (u < 0.f || v < 0.f) continue;
            const int iu = static_cast<int>(u), iv = static_cast<int>(v);
            if (iu >= bmp.w || iv >= bmp.h) continue;
            const uint32_t p = bmp.get(iu, iv);
            if (p >> 24) pixels_[static_cast<size_t>(y) * w_ + x] = p;
        }
    }
}

void Framebuffer::blit(const Framebuffer& src, int x, int y) {
    const int x0 = std::max(clip_x0_, x), x1 = std::min(clip_x1_, x + src.w_);
    const int y0 = std::max(clip_y0_, y), y1 = std::min(clip_y1_, y + src.h_);
    if (x1 <= x0) return;
    for (int row = y0; row < y1; ++row) {
        const uint32_t* from = &src.pixels_[static_cast<size_t>(row - y) * src.w_ + (x0 - x)];
        std::copy(from, from + (x1 - x0), &pixels_[static_cast<size_t>(row) * w_ + x0]);
    }
}

void Framebuffer::line(int x0, int y0, int x1, int y1, Color c) {
    const int steps = std::max(std::abs(x1 - x0), std::abs(y1 - y0));
    for (int i = 0; i <= steps; ++i) {
        const float t = steps ? static_cast<float>(i) / static_cast<float>(steps) : 0.f;
        put_pixel(static_cast<int>(std::lround(static_cast<float>(x0) + static_cast<float>(x1 - x0) * t)),
                  static_cast<int>(std::lround(static_cast<float>(y0) + static_cast<float>(y1 - y0) * t)), c);
    }
}

void Framebuffer::fill_trapezoid(float y_top, float xl_top, float xr_top,
                                 float y_bot, float xl_bot, float xr_bot, Color c) {
    if (!(y_bot > y_top)) return;
    int row0 = std::max(clip_y0_, pixel_edge(y_top));
    int row1 = std::min(clip_y1_, pixel_edge(y_bot));
    // Sub-pixel spans can round to zero rows and leave a 1px gap between segments.
    if (row0 >= row1) {
        const int mid = pixel_edge(0.5f * (y_top + y_bot));
        if (mid < clip_y0_ || mid >= clip_y1_) return;
        row0 = mid;
        row1 = mid + 1;
    }
    const float inv_h = 1.f / (y_bot - y_top);
    const uint32_t argb = c.argb();
    const int clip_l = clip_x0_, clip_r = clip_x1_;
    const int stride = w_;
    uint32_t* base = pixels_.data();
    for (int y = row0; y < row1; ++y) {
        const float t = (static_cast<float>(y) + 0.5f - y_top) * inv_h;
        int x0 = pixel_edge(xl_top + (xl_bot - xl_top) * t);
        int x1 = pixel_edge(xr_top + (xr_bot - xr_top) * t);
        if (x0 < clip_l) x0 = clip_l;
        if (x1 > clip_r) x1 = clip_r;
        if (x0 >= x1) continue;
        uint32_t* row = base + static_cast<size_t>(y) * static_cast<size_t>(stride);
        std::fill(row + x0, row + x1, argb);
    }
}

void Framebuffer::draw_text(int x, int y, std::string_view s, Color c, int scale) {
    font::render(x, y, s, scale, [&](int px, int py) { put_pixel(px, py, c); });
}

void Framebuffer::blit_scaled(const Bitmap& bmp, float x, float y, float w, float h,
                              bool flip, float fog_amount, Color fog) {
    if (bmp.w == 0 || bmp.h == 0 || !(w > 0.f) || !(h > 0.f)) return;
    const int x0 = std::max(clip_x0_, pixel_edge(x));
    const int x1 = std::min(clip_x1_, pixel_edge(x + w));
    const int y0 = std::max(clip_y0_, pixel_edge(y));
    const int y1 = std::min(clip_y1_, pixel_edge(y + h));
    if (x0 >= x1 || y0 >= y1) return;

    const float sx = static_cast<float>(bmp.w) / w;
    const float sy = static_cast<float>(bmp.h) / h;
    const int span = x1 - x0;
    columns_.resize(static_cast<size_t>(span));
    int* cols = columns_.data();
    const int bmp_w = bmp.w;
    for (int i = 0; i < span; ++i) {
        int u = static_cast<int>((static_cast<float>(x0 + i) + 0.5f - x) * sx);
        if (u < 0) u = 0;
        else if (u >= bmp_w) u = bmp_w - 1;
        cols[i] = flip ? bmp_w - 1 - u : u;
    }

    const bool fogged = fog_amount > 0.004f;
    const int bmp_h = bmp.h;
    const uint32_t* bmp_px = bmp.px.data();
    uint32_t* fb = pixels_.data();
    const int stride = w_;
    if (!fogged) {
        // Opaque path: no Color/blend construction.
        for (int dy = y0; dy < y1; ++dy) {
            int v = static_cast<int>((static_cast<float>(dy) + 0.5f - y) * sy);
            if (v < 0) v = 0;
            else if (v >= bmp_h) v = bmp_h - 1;
            const uint32_t* src = bmp_px + static_cast<size_t>(v) * static_cast<size_t>(bmp_w);
            uint32_t* dst = fb + static_cast<size_t>(dy) * static_cast<size_t>(stride);
            for (int i = 0; i < span; ++i) {
                const uint32_t p = src[cols[i]];
                if (p & 0xff000000u) dst[x0 + i] = p;
            }
        }
        return;
    }
    const uint32_t fog_argb = fog.argb();
    const float keep = 1.f - fog_amount;
    for (int dy = y0; dy < y1; ++dy) {
        int v = static_cast<int>((static_cast<float>(dy) + 0.5f - y) * sy);
        if (v < 0) v = 0;
        else if (v >= bmp_h) v = bmp_h - 1;
        const uint32_t* src = bmp_px + static_cast<size_t>(v) * static_cast<size_t>(bmp_w);
        uint32_t* dst = fb + static_cast<size_t>(dy) * static_cast<size_t>(stride);
        for (int i = 0; i < span; ++i) {
            const uint32_t p = src[cols[i]];
            if ((p >> 24) == 0) continue;
            const int pr = static_cast<int>((p >> 16) & 0xff);
            const int pg = static_cast<int>((p >> 8) & 0xff);
            const int pb = static_cast<int>(p & 0xff);
            // Lights keep through fog, mostly — a red body panel must fog
            // like the rest of the sprite.
            float fa = fog_amount;
            float kp = keep;
            const bool glow = is_glowing(p);
            if (glow) {
                fa = fog_amount * 0.18f;
                kp = 1.f - fa;
            }
            const int fr = static_cast<int>((fog_argb >> 16) & 0xff);
            const int fg = static_cast<int>((fog_argb >> 8) & 0xff);
            const int fb_c = static_cast<int>(fog_argb & 0xff);
            const int r = static_cast<int>(static_cast<float>(pr) * kp + static_cast<float>(fr) * fa + 0.5f);
            const int g = static_cast<int>(static_cast<float>(pg) * kp + static_cast<float>(fg) * fa + 0.5f);
            const int b = static_cast<int>(static_cast<float>(pb) * kp + static_cast<float>(fb_c) * fa + 0.5f);
            dst[x0 + i] = (glow ? static_cast<uint32_t>(glow_alpha) << 24 : 0xff000000u) |
                          static_cast<uint32_t>(r) << 16 | static_cast<uint32_t>(g) << 8 | static_cast<uint32_t>(b);
        }
    }
}

} // namespace racer
