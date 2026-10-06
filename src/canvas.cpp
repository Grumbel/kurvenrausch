// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "canvas.hpp"

#include "font.hpp"

#include <algorithm>
#include <cmath>

namespace racer {

// --- FbCanvas -----------------------------------------------------------

void FbCanvas::set_clip(int x0, int y0, int x1, int y1) {
    fb_.set_clip(x0, y0, x1, y1);
    clip_x0_ = std::clamp(x0, 0, fb_.width());
    clip_y0_ = std::clamp(y0, 0, fb_.height());
    clip_x1_ = std::clamp(x1, clip_x0_, fb_.width());
    clip_y1_ = std::clamp(y1, clip_y0_, fb_.height());
}

void FbCanvas::reset_clip() {
    fb_.reset_clip();
    clip_x0_ = clip_y0_ = 0;
    clip_x1_ = clip_y1_ = 1 << 30;
}

void FbCanvas::blend_rect(int x, int y, int w, int h, Color c, float alpha) {
    const int x0 = std::max({x, clip_x0_, 0}), x1 = std::min({x + w, clip_x1_, fb_.width()});
    const int y0 = std::max({y, clip_y0_, 0}), y1 = std::min({y + h, clip_y1_, fb_.height()});
    if (x0 >= x1 || y0 >= y1) return;
    const float a = std::clamp(alpha, 0.f, 1.f);
    // Over opaque pixels (all of the software picture) the same src-over as
    // Framebuffer::blend_pixel, without its per-pixel alpha bookkeeping.
    const float sr = static_cast<float>(c.r) * a, sg = static_cast<float>(c.g) * a, sb = static_cast<float>(c.b) * a;
    const float keep = 1.f - a;
    uint32_t* px = fb_.pixels_mut();
    for (int j = y0; j < y1; ++j) {
        uint32_t* row = px + static_cast<size_t>(j) * static_cast<size_t>(fb_.width());
        for (int i = x0; i < x1; ++i) {
            const uint32_t d = row[i];
            if ((d >> 24) != 0xff) {
                fb_.blend_pixel(i, j, c, a);
                continue;
            }
            const auto ch = [keep](float s, uint32_t dc) {
                return static_cast<uint32_t>(s + static_cast<float>(dc) * keep + 0.5f);
            };
            row[i] = 0xff000000u | ch(sr, (d >> 16) & 0xff) << 16 | ch(sg, (d >> 8) & 0xff) << 8 | ch(sb, d & 0xff);
        }
    }
}

void FbCanvas::blit(const Bitmap& bmp, float sx0, float sy0, float sx1, float sy1, float x, float y, float w, float h,
                    bool flip) {
    if (bmp.w <= 0 || bmp.h <= 0 || !(w > 0.f) || !(h > 0.f)) return;
    const int x0 = std::max(0, pixel_edge(x)), x1 = std::min(fb_.width(), pixel_edge(x + w));
    const int y0 = std::max(0, pixel_edge(y)), y1 = std::min(fb_.height(), pixel_edge(y + h));
    for (int j = y0; j < y1; ++j) {
        const float tv = (static_cast<float>(j) + 0.5f - y) / h;
        const int v = std::clamp(static_cast<int>(sy0 == sy1 ? sy0 : sy0 + tv * (sy1 - sy0)), 0, bmp.h - 1);
        for (int i = x0; i < x1; ++i) {
            float tu = (static_cast<float>(i) + 0.5f - x) / w;
            if (flip) tu = 1.f - tu;
            const int u = std::clamp(static_cast<int>(sx0 == sx1 ? sx0 : sx0 + tu * (sx1 - sx0)), 0, bmp.w - 1);
            const uint32_t p = bmp.get(u, v);
            if (p >> 24) fb_.put_pixel(i, j, Color{static_cast<uint8_t>(p >> 16), static_cast<uint8_t>(p >> 8),
                                                  static_cast<uint8_t>(p)});
        }
    }
}

void FbCanvas::dither_disc(int cx, int cy, int r, Color c, float threshold, float alpha) {
    for (int dy = -r; dy <= r; ++dy) {
        for (int dx = -r; dx <= r; ++dx) {
            if (dx * dx + dy * dy > r * r) continue;
            const int px = cx + dx, py = cy + dy;
            if (bayer4(px, py) < threshold) fb_.blend_pixel(px, py, c, alpha);
        }
    }
}

// --- font atlas ---------------------------------------------------------

namespace font_atlas {

Bitmap make() {
    Bitmap b(width, height);
    for (int code = 32; code < 127; ++code) {
        const uint8_t* rows = font::glyph(static_cast<char>(code));
        const int ox = (code % columns) * cell, oy = (code / columns) * cell;
        for (int gy = 0; gy < font::glyph_h; ++gy)
            for (int gx = 0; gx < font::glyph_w; ++gx)
                if (rows[gy] & (0x10 >> gx)) b.px[static_cast<size_t>(oy + gy) * width + ox + gx] = 0xffffffffu;
    }
    const int wx = (white_cell % columns) * cell, wy = (white_cell / columns) * cell;
    for (int y = 0; y < cell; ++y)
        for (int x = 0; x < cell; ++x) b.px[static_cast<size_t>(wy + y) * width + wx + x] = 0xffffffffu;
    return b;
}

} // namespace font_atlas

// --- DrawList -----------------------------------------------------------

namespace {

// The centre of the white cell, in atlas texels.
constexpr float white_u = static_cast<float>((font_atlas::white_cell % font_atlas::columns) * font_atlas::cell) + 4.f;
constexpr float white_v = static_cast<float>((font_atlas::white_cell / font_atlas::columns) * font_atlas::cell) + 4.f;

} // namespace

void DrawList::clear() {
    verts_.clear();
    runs_.clear();
    layer_ = 0;
    dynamic_ = false;
    merge_open_ = false;
    reset_clip();
}

void DrawList::resize(int width, int height) {
    w_ = width;
    h_ = height;
    clear();
}

void DrawList::set_layer(int layer) {
    if (layer == layer_) return;
    layer_ = layer;
    merge_open_ = false;
}

void DrawList::set_clip(int x0, int y0, int x1, int y1) {
    clip_x0_ = std::clamp(x0, 0, w_);
    clip_y0_ = std::clamp(y0, 0, h_);
    clip_x1_ = std::clamp(x1, clip_x0_, w_);
    clip_y1_ = std::clamp(y1, clip_y0_, h_);
}

void DrawList::reset_clip() { set_clip(0, 0, w_, h_); }

DrawList::Run& DrawList::run_for(Source source, const racer::Bitmap* bmp, unsigned texture) {
    const bool dynamic = source == Source::Bitmap && dynamic_;
    if (runs_.empty() || runs_.back().source != source || runs_.back().bitmap != bmp ||
        runs_.back().texture != texture || runs_.back().layer != layer_ || runs_.back().dynamic != dynamic) {
        Run r;
        r.source = source;
        r.bitmap = bmp;
        r.texture = texture;
        r.dynamic = dynamic;
        r.layer = layer_;
        r.first = verts_.size();
        runs_.push_back(r);
    }
    return runs_.back();
}

void DrawList::quad(float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1, Color c,
                    float alpha, Source source, const racer::Bitmap* bmp, unsigned texture) {
    const float cx0 = std::max(x0, static_cast<float>(clip_x0_)), cx1 = std::min(x1, static_cast<float>(clip_x1_));
    const float cy0 = std::max(y0, static_cast<float>(clip_y0_)), cy1 = std::min(y1, static_cast<float>(clip_y1_));
    if (!(cx1 > cx0) || !(cy1 > cy0)) return;
    // Texture coordinates follow the clipped edges.
    const auto lerp = [](float a, float b, float t) { return a + (b - a) * t; };
    const float tx0 = (cx0 - x0) / (x1 - x0), tx1 = (cx1 - x0) / (x1 - x0);
    const float ty0 = (cy0 - y0) / (y1 - y0), ty1 = (cy1 - y0) / (y1 - y0);
    const float qu0 = lerp(u0, u1, tx0), qu1 = lerp(u0, u1, tx1);
    const float qv0 = lerp(v0, v1, ty0), qv1 = lerp(v0, v1, ty1);
    Run& run = run_for(source, bmp, texture);
    const float r = c.r / 255.f, g = c.g / 255.f, b = c.b / 255.f, a = std::clamp(alpha, 0.f, 1.f);
    const Vertex tl{cx0, cy0, qu0, qv0, r, g, b, a, 0.f, 0.f, 0.f, 0.f};
    const Vertex tr{cx1, cy0, qu1, qv0, r, g, b, a, 0.f, 0.f, 0.f, 0.f};
    const Vertex bl{cx0, cy1, qu0, qv1, r, g, b, a, 0.f, 0.f, 0.f, 0.f};
    const Vertex br{cx1, cy1, qu1, qv1, r, g, b, a, 0.f, 0.f, 0.f, 0.f};
    const Vertex v[6] = {tl, tr, bl, tr, br, bl};
    verts_.insert(verts_.end(), v, v + 6);
    run.count += 6;
    merge_open_ = false;
}

void DrawList::solid(int x, int y, int w, int h, Color c, float alpha) {
    if (w <= 0 || h <= 0) return;
    // Extend the previous solid quad when this one continues it on the right.
    if (merge_open_ && y == merge_y_ && h == merge_h_ && x == merge_x1_ && c.argb() == merge_color_ &&
        alpha == merge_alpha_ && !runs_.empty() && runs_.back().source == Source::Font) {
        const float nx1 = std::min(static_cast<float>(x + w), static_cast<float>(clip_x1_));
        if (nx1 > static_cast<float>(x) && y >= clip_y0_ && y + h <= clip_y1_ && x >= clip_x0_) {
            Vertex* q = &verts_[verts_.size() - 6];
            q[1].x = q[3].x = q[4].x = nx1; // tr, tr, br
            merge_x1_ = x + w;
            return;
        }
    }
    quad(static_cast<float>(x), static_cast<float>(y), static_cast<float>(x + w), static_cast<float>(y + h), white_u,
         white_v, white_u, white_v, c, alpha, Source::Font);
    // Only an unclipped quad can grow (its right edge is x + w).
    const bool whole = x >= clip_x0_ && y >= clip_y0_ && x + w <= clip_x1_ && y + h <= clip_y1_;
    if (whole && !verts_.empty()) {
        merge_open_ = true;
        merge_x1_ = x + w;
        merge_y_ = y;
        merge_h_ = h;
        merge_color_ = c.argb();
        merge_alpha_ = alpha;
    }
}

void DrawList::fill_rect(int x, int y, int w, int h, Color c) { solid(x, y, w, h, c, 1.f); }

void DrawList::blend_rect(int x, int y, int w, int h, Color c, float alpha) { solid(x, y, w, h, c, alpha); }

void DrawList::line(int x0, int y0, int x1, int y1, Color c) {
    // The same pixels as Framebuffer::line.
    const int steps = std::max(std::abs(x1 - x0), std::abs(y1 - y0));
    for (int i = 0; i <= steps; ++i) {
        const float t = steps ? static_cast<float>(i) / static_cast<float>(steps) : 0.f;
        fill_rect(static_cast<int>(std::lround(static_cast<float>(x0) + static_cast<float>(x1 - x0) * t)),
                  static_cast<int>(std::lround(static_cast<float>(y0) + static_cast<float>(y1 - y0) * t)), 1, 1, c);
    }
}

void DrawList::draw_text(int x, int y, std::string_view s, Color c, int scale) {
    const float gw = static_cast<float>(font::glyph_w * scale), gh = static_cast<float>(font::glyph_h * scale);
    for (const char ch : s) {
        const auto code = static_cast<unsigned char>(ch);
        if (code > 32 && code < 127) {
            const float u = static_cast<float>((code % font_atlas::columns) * font_atlas::cell);
            const float v = static_cast<float>((code / font_atlas::columns) * font_atlas::cell);
            quad(static_cast<float>(x), static_cast<float>(y), static_cast<float>(x) + gw, static_cast<float>(y) + gh, u,
                 v, u + font::glyph_w, v + font::glyph_h, c, 1.f, Source::Font);
        }
        x += font::advance * scale;
    }
}

void DrawList::blit(const Bitmap& bmp, float sx0, float sy0, float sx1, float sy1, float x, float y, float w, float h,
                    bool flip) {
    if (bmp.w <= 0 || bmp.h <= 0 || !(w > 0.f) || !(h > 0.f)) return;
    // Snap to the pixels the software blit covers (centres inside).
    const float x0 = static_cast<float>(pixel_edge(x)), x1 = static_cast<float>(pixel_edge(x + w));
    const float y0 = static_cast<float>(pixel_edge(y)), y1 = static_cast<float>(pixel_edge(y + h));
    if (!(x1 > x0) || !(y1 > y0)) return;
    // Source coordinates at the snapped edges; a zero-width source samples
    // the middle of its texel.
    const auto at = [](float s0, float s1, float t) { return s0 == s1 ? s0 + 0.5f : s0 + (s1 - s0) * t; };
    float u0 = at(sx0, sx1, (x0 - x) / w), u1 = at(sx0, sx1, (x1 - x) / w);
    if (flip) {
        u0 = at(sx0, sx1, 1.f - (x0 - x) / w);
        u1 = at(sx0, sx1, 1.f - (x1 - x) / w);
    }
    const float v0 = at(sy0, sy1, (y0 - y) / h), v1 = at(sy0, sy1, (y1 - y) / h);
    quad(x0, y0, x1, y1, u0, v0, u1, v1, Color{255, 255, 255}, 1.f, Source::Bitmap, &bmp);
}

void DrawList::blit_rotated(const Bitmap& bmp, float cx, float cy, float w, float h, float angle) {
    if (bmp.w <= 0 || bmp.h <= 0 || !(w > 0.f) || !(h > 0.f)) return;
    Run& run = run_for(Source::Bitmap, &bmp, 0);
    const float c = std::cos(angle), s = std::sin(angle);
    const float hw = 0.5f * w, hh = 0.5f * h;
    const auto corner = [&](float lx, float ly, float u, float v) {
        return Vertex{cx + lx * c - ly * s, cy + lx * s + ly * c, u, v, 1.f, 1.f, 1.f, 1.f, 0.f, 0.f, 0.f, 0.f};
    };
    const float bw = static_cast<float>(bmp.w), bh = static_cast<float>(bmp.h);
    const Vertex tl = corner(-hw, -hh, 0.f, 0.f), tr = corner(hw, -hh, bw, 0.f);
    const Vertex bl = corner(-hw, hh, 0.f, bh), br = corner(hw, hh, bw, bh);
    const Vertex v[6] = {tl, tr, bl, tr, br, bl};
    verts_.insert(verts_.end(), v, v + 6);
    run.count += 6;
    merge_open_ = false;
}

void DrawList::dither_disc(int cx, int cy, int r, Color c, float threshold, float alpha) {
    if (r < 0) return;
    const float x0 = std::max(static_cast<float>(cx - r), static_cast<float>(clip_x0_));
    const float x1 = std::min(static_cast<float>(cx + r + 1), static_cast<float>(clip_x1_));
    const float y0 = std::max(static_cast<float>(cy - r), static_cast<float>(clip_y0_));
    const float y1 = std::min(static_cast<float>(cy + r + 1), static_cast<float>(clip_y1_));
    if (!(x1 > x0) || !(y1 > y0)) return;
    Run& run = run_for(Source::Font, nullptr, 0);
    const float rr = c.r / 255.f, gg = c.g / 255.f, bb = c.b / 255.f, a = std::clamp(alpha, 0.f, 1.f);
    // The shader tests each pixel against the disc; radius + 0.5 keeps
    // radius 0 (a single pixel) apart from plain quads.
    const float fx = static_cast<float>(cx), fy = static_cast<float>(cy), rad = static_cast<float>(r) + 0.5f;
    const auto vert = [&](float x, float y) {
        return Vertex{x, y, white_u, white_v, rr, gg, bb, a, fx, fy, rad, threshold};
    };
    const Vertex v[6] = {vert(x0, y0), vert(x1, y0), vert(x0, y1), vert(x1, y0), vert(x1, y1), vert(x0, y1)};
    verts_.insert(verts_.end(), v, v + 6);
    run.count += 6;
    merge_open_ = false;
}

void DrawList::blit_texture(unsigned texture, float x, float y, float w, float h) {
    // FBO textures are stored bottom row first.
    quad(x, y, x + w, y + h, 0.f, 1.f, 1.f, 0.f, Color{255, 255, 255}, 1.f, Source::Texture, nullptr, texture);
}

} // namespace racer
