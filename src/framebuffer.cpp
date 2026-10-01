#include "framebuffer.hpp"

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

void Framebuffer::fill_trapezoid(float y_top, float xl_top, float xr_top,
                                 float y_bot, float xl_bot, float xr_bot, Color c) {
    if (!(y_bot > y_top)) return;
    const int row0 = std::max(clip_y0_, pixel_edge(y_top));
    const int row1 = std::min(clip_y1_, pixel_edge(y_bot));
    const float inv_h = 1.f / (y_bot - y_top);
    for (int y = row0; y < row1; ++y) {
        const float t = (static_cast<float>(y) + 0.5f - y_top) * inv_h;
        const float xl = xl_top + (xl_bot - xl_top) * t;
        const float xr = xr_top + (xr_bot - xr_top) * t;
        hline(pixel_edge(xl), pixel_edge(xr), y, c);
    }
}

} // namespace racer
