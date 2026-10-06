// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "bitmap.hpp"
#include "framebuffer.hpp"
#include "types.hpp"

#include <cstddef>
#include <cstdint>
#include <string_view>
#include <vector>

namespace racer {

// 2D drawing in framebuffer pixels for everything over the road scene (HUD,
// menus, particles, cockpit, mirror). FbCanvas draws straight into a
// Framebuffer (the software path); DrawList records quads for the GPU.
// Clip rectangles are half-open, [x0, x1) x [y0, y1).
class Canvas {
public:
    virtual ~Canvas() = default;

    virtual int width() const = 0;
    virtual int height() const = 0;
    virtual void set_clip(int x0, int y0, int x1, int y1) = 0;
    virtual void reset_clip() = 0;

    virtual void fill_rect(int x, int y, int w, int h, Color c) = 0;
    // c over what is there with opacity alpha (0 .. 1).
    virtual void blend_rect(int x, int y, int w, int h, Color c, float alpha) = 0;
    // Line from (x0, y0) to (x1, y1), both ends included.
    virtual void line(int x0, int y0, int x1, int y1, Color c) = 0;
    // Text in the 5x7 bitmap font, top-left at (x, y).
    virtual void draw_text(int x, int y, std::string_view s, Color c, int scale = 1) = 0;
    // The bitmap's texels [sx0, sx1) x [sy0, sy1) scaled onto the rectangle
    // (x, y, w, h), mirrored if flip; transparent texels are skipped. A source
    // span of zero width stretches that one texel column (or row).
    virtual void blit(const Bitmap& bmp, float sx0, float sy0, float sx1, float sy1, float x, float y, float w,
                      float h, bool flip = false) = 0;
    // The whole bitmap scaled to w x h, rotated by angle (radians, clockwise)
    // about (cx, cy). Not clipped.
    virtual void blit_rotated(const Bitmap& bmp, float cx, float cy, float w, float h, float angle) = 0;
    // The pixels within radius r of (cx, cy) whose bayer4() is below
    // threshold, c blended with opacity alpha (dust and smoke puffs).
    virtual void dither_disc(int cx, int cy, int r, Color c, float threshold, float alpha) = 0;

    void put_pixel(int x, int y, Color c) { fill_rect(x, y, 1, 1, c); }
    void blend_pixel(int x, int y, Color c, float alpha) { blend_rect(x, y, 1, 1, c, alpha); }
    void hline(int x0, int x1, int y, Color c) { fill_rect(x0, y, x1 - x0, 1, c); }
    void blit(const Bitmap& bmp, float x, float y, float w, float h, bool flip = false) {
        blit(bmp, 0.f, 0.f, static_cast<float>(bmp.w), static_cast<float>(bmp.h), x, y, w, h, flip);
    }
};

class FbCanvas final : public Canvas {
public:
    explicit FbCanvas(Framebuffer& fb) : fb_(fb) {}

    int width() const override { return fb_.width(); }
    int height() const override { return fb_.height(); }
    void set_clip(int x0, int y0, int x1, int y1) override;
    void reset_clip() override;
    void fill_rect(int x, int y, int w, int h, Color c) override { fb_.fill_rect(x, y, w, h, c); }
    void blend_rect(int x, int y, int w, int h, Color c, float alpha) override;
    void line(int x0, int y0, int x1, int y1, Color c) override { fb_.line(x0, y0, x1, y1, c); }
    void draw_text(int x, int y, std::string_view s, Color c, int scale = 1) override {
        fb_.draw_text(x, y, s, c, scale);
    }
    using Canvas::blit;
    void blit(const Bitmap& bmp, float sx0, float sy0, float sx1, float sy1, float x, float y, float w, float h,
              bool flip = false) override;
    void blit_rotated(const Bitmap& bmp, float cx, float cy, float w, float h, float angle) override {
        fb_.blit_rotated(bmp, cx, cy, w, h, angle);
    }
    void dither_disc(int cx, int cy, int r, Color c, float threshold, float alpha) override;

private:
    Framebuffer& fb_;
    int clip_x0_ = 0, clip_y0_ = 0, clip_x1_ = 1 << 30, clip_y1_ = 1 << 30;
};

// The glyph atlas DrawList text refers to: 16 x 8 cells of 8 x 8 texels, the
// glyph of character code i at cell (i % 16, i / 16), its top-left texel at
// the cell's corner. Cell 127 is solid white, for untextured quads.
namespace font_atlas {
constexpr int cell = 8;
constexpr int columns = 16;
constexpr int width = columns * cell;
constexpr int height = 8 * cell;
constexpr int white_cell = 127;
Bitmap make();
} // namespace font_atlas

// Quads for the GPU, in framebuffer pixels, batched into runs that share a
// texture. Adjacent same-coloured spans on a row merge into one quad, so
// pixel-by-pixel drawing stays cheap. Clipping is done here, on the CPU.
class DrawList final : public Canvas {
public:
    struct Vertex {
        float x, y;     // framebuffer pixels
        float u, v;     // texels of the source (Font, Bitmap) or 0..1 (Texture)
        float r, g, b, a;
        // Dither discs: centre, radius and threshold (radius 0: plain texel).
        float cx, cy, radius, threshold;
    };
    enum class Source : uint8_t {
        Font,    // the font atlas (and its white cell)
        Bitmap,  // a CPU bitmap, uploaded and cached by the renderer
        Texture, // a GL texture name, sampled with V = 0 at the bottom (an FBO)
    };
    struct Run {
        Source source = Source::Font;
        const racer::Bitmap* bitmap = nullptr;
        unsigned texture = 0;
        bool dynamic = false; // bitmap pixels change between frames
        int layer = 0;
        size_t first = 0, count = 0; // vertices
    };

    DrawList(int width, int height) : w_(width), h_(height) { reset_clip(); }

    void clear();
    void resize(int width, int height);
    // Subsequent draws go to this layer (the renderer decides what a layer
    // means, e.g. under or over the weather).
    void set_layer(int layer);
    // Subsequent Bitmap blits re-upload their pixels each frame.
    void set_dynamic(bool dynamic) { dynamic_ = dynamic; }

    int width() const override { return w_; }
    int height() const override { return h_; }
    void set_clip(int x0, int y0, int x1, int y1) override;
    void reset_clip() override;
    void fill_rect(int x, int y, int w, int h, Color c) override;
    void blend_rect(int x, int y, int w, int h, Color c, float alpha) override;
    void line(int x0, int y0, int x1, int y1, Color c) override;
    void draw_text(int x, int y, std::string_view s, Color c, int scale = 1) override;
    using Canvas::blit;
    void blit(const Bitmap& bmp, float sx0, float sy0, float sx1, float sy1, float x, float y, float w, float h,
              bool flip = false) override;
    void blit_rotated(const Bitmap& bmp, float cx, float cy, float w, float h, float angle) override;
    void dither_disc(int cx, int cy, int r, Color c, float threshold, float alpha) override;
    // A GL texture (whole) onto the rectangle, e.g. a render target.
    void blit_texture(unsigned texture, float x, float y, float w, float h);

    const std::vector<Vertex>& vertices() const { return verts_; }
    const std::vector<Run>& runs() const { return runs_; }
    bool empty() const { return verts_.empty(); }

private:
    Run& run_for(Source source, const racer::Bitmap* bmp, unsigned texture);
    // Clipped axis-aligned quad; u/v interpolate across it.
    void quad(float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1, Color c, float alpha,
              Source source, const racer::Bitmap* bmp = nullptr, unsigned texture = 0);
    void solid(int x, int y, int w, int h, Color c, float alpha);

    int w_, h_;
    int clip_x0_ = 0, clip_y0_ = 0, clip_x1_ = 0, clip_y1_ = 0;
    int layer_ = 0;
    bool dynamic_ = false;
    std::vector<Vertex> verts_;
    std::vector<Run> runs_;
    // The last solid quad, for merging the next one onto its right end.
    bool merge_open_ = false;
    int merge_x1_ = 0, merge_y_ = 0, merge_h_ = 0;
    uint32_t merge_color_ = 0;
    float merge_alpha_ = 0.f;
};

} // namespace racer
