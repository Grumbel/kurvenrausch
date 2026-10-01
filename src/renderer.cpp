#include "renderer.hpp"
#include <algorithm>
#include <cstring>
#include <cmath>

namespace racer {

Renderer::Renderer(int width, int height) : w_(width), h_(height) {
    pixels_.resize(static_cast<size_t>(w_ * h_), 0);
}

void Renderer::begin_frame() {
    std::fill(pixels_.begin(), pixels_.end(), 0u);
}

void Renderer::put_pixel(int x, int y, Color c) {
    if (x < 0 || x >= w_ || y < 0 || y >= h_) return;
    pixels_[static_cast<size_t>(y * w_ + x)] = c.argb();
}

void Renderer::hline(int x1, int x2, int y, Color c) {
    if (y < 0 || y >= h_) return;
    if (x1 > x2) std::swap(x1, x2);
    x1 = std::max(0, x1);
    x2 = std::min(w_ - 1, x2);
    uint32_t col = c.argb();
    uint32_t* row = &pixels_[static_cast<size_t>(y * w_)];
    for (int x = x1; x <= x2; ++x) row[x] = col;
}

void Renderer::fill_rect(int x, int y, int ww, int hh, Color c) {
    for (int j = 0; j < hh; ++j)
        hline(x, x + ww - 1, y + j, c);
}

void Renderer::draw_background(float sky_offset, float /*hill_offset*/) {
    // Classic vertical gradient sky + distant hills
    for (int y = 0; y < h_ / 2; ++y) {
        float t = static_cast<float>(y) / (h_ / 2.f);
        Color sky{
            static_cast<uint8_t>(Palette::SkyTop.r + t * (Palette::SkyBottom.r - Palette::SkyTop.r)),
            static_cast<uint8_t>(Palette::SkyTop.g + t * (Palette::SkyBottom.g - Palette::SkyTop.g)),
            static_cast<uint8_t>(Palette::SkyTop.b + t * (Palette::SkyBottom.b - Palette::SkyTop.b))
        };
        hline(0, w_ - 1, y, sky);
    }
    // Simple parallax distant ground strip
    int horizon = h_ / 2;
    for (int y = horizon; y < horizon + 40; ++y) {
        float t = static_cast<float>(y - horizon) / 40.f;
        Color ground{
            static_cast<uint8_t>(0x60 + t * 0x20),
            static_cast<uint8_t>(0x80 + t * 0x10),
            static_cast<uint8_t>(0x40)
        };
        // slight horizontal shift for parallax
        int shift = static_cast<int>(sky_offset * 0.1f) % w_;
        hline(0, w_ - 1, y, ground);
        (void)shift;
    }
}

void Renderer::draw_segment(const Projected& p1, const Projected& p2,
                            const Color& road, const Color& grass,
                            const Color& rumble, const Color& lane,
                            int lanes, float rumble_width) {
    // Classic trapezoid / scanline road: interpolate x and w between p1 and p2
    auto draw_poly = [&](float x1, float y1, float w1, float x2, float y2, float w2, Color c) {
        int iy1 = static_cast<int>(y1);
        int iy2 = static_cast<int>(y2);
        if (iy1 == iy2) return;
        if (iy1 > iy2) {
            std::swap(iy1, iy2);
            std::swap(x1, x2);
            std::swap(w1, w2);
        }
        float dy = static_cast<float>(iy2 - iy1);
        for (int y = iy1; y <= iy2; ++y) {
            float t = (y - iy1) / dy;
            float x = x1 + t * (x2 - x1);
            float ww = w1 + t * (w2 - w1);
            hline(static_cast<int>(x - ww), static_cast<int>(x + ww), y, c);
        }
    };

    // Grass (full width)
    draw_poly(0, p1.y, w_, 0, p2.y, w_, grass);

    // Rumble strips
    float r1 = p1.w * rumble_width;
    float r2 = p2.w * rumble_width;
    draw_poly(p1.x - p1.w - r1, p1.y, r1, p2.x - p2.w - r2, p2.y, r2, rumble);
    draw_poly(p1.x + p1.w + r1, p1.y, r1, p2.x + p2.w + r2, p2.y, r2, rumble);

    // Road
    draw_poly(p1.x, p1.y, p1.w, p2.x, p2.y, p2.w, road);

    // Lane markers (center lines)
    if (lanes > 1) {
        float lane_w1 = p1.w * 0.02f;
        float lane_w2 = p2.w * 0.02f;
        for (int i = 1; i < lanes; ++i) {
            float frac = static_cast<float>(i) / lanes;
            float lx1 = p1.x - p1.w + 2.f * p1.w * frac;
            float lx2 = p2.x - p2.w + 2.f * p2.w * frac;
            draw_poly(lx1, p1.y, lane_w1, lx2, p2.y, lane_w2, lane);
        }
    }
}

void Renderer::draw_sprite(float screen_x, float screen_y, float scale,
                           int type, bool flip) {
    // Procedural classic pixel sprites (tree / cliff / rock)
    int base_w = 64;
    int base_h = 96;
    int sw = static_cast<int>(base_w * scale);
    int sh = static_cast<int>(base_h * scale);
    if (sw < 2 || sh < 2) return;

    int sx = static_cast<int>(screen_x - sw / 2);
    int sy = static_cast<int>(screen_y - sh);

    if (type == 0) { // Tree
        // Trunk
        int tw = sw / 5;
        fill_rect(sx + sw / 2 - tw / 2, sy + sh / 2, tw, sh / 2, Palette::TreeTrunk);
        // Foliage (triangle-ish)
        for (int y = 0; y < sh / 2; ++y) {
            float t = 1.f - static_cast<float>(y) / (sh / 2.f);
            int hw = static_cast<int>(sw / 2 * t);
            hline(sx + sw / 2 - hw, sx + sw / 2 + hw, sy + y, Palette::TreeGreen);
        }
    } else if (type == 1) { // Cliff / rock face
        for (int y = 0; y < sh; ++y) {
            float t = static_cast<float>(y) / sh;
            int hw = static_cast<int>(sw * (0.4f + 0.6f * t));
            Color c{
                static_cast<uint8_t>(Palette::Cliff.r + (y % 8) * 2),
                static_cast<uint8_t>(Palette::Cliff.g + (y % 5)),
                static_cast<uint8_t>(Palette::Cliff.b)
            };
            int cx = flip ? sx + sw - hw : sx;
            hline(cx, cx + hw, sy + y, c);
        }
    } else { // Billboard / rock
        fill_rect(sx, sy, sw, sh, Color{0x80, 0x80, 0x60});
        fill_rect(sx + 2, sy + 2, sw - 4, sh - 4, Color{0xA0, 0xA0, 0x80});
    }
}

void Renderer::draw_player_car(float x, float y, float steer, float speed) {
    // Classic low-poly / pixel car viewed from behind
    int cx = static_cast<int>(x);
    int cy = static_cast<int>(y);
    float lean = steer * 8.f; // visual lean

    // Body
    int bw = 80, bh = 40;
    fill_rect(cx - bw / 2 + static_cast<int>(lean), cy - bh, bw, bh, Palette::CarRed);
    // Roof
    fill_rect(cx - bw / 3 + static_cast<int>(lean * 0.5f), cy - bh - 18, bw * 2 / 3, 20, Palette::CarDark);
    // Wheels
    fill_rect(cx - bw / 2 - 4, cy - 12, 12, 16, Palette::Black);
    fill_rect(cx + bw / 2 - 8, cy - 12, 12, 16, Palette::Black);
    // Headlights hint
    if (speed > 50.f) {
        put_pixel(cx - 20, cy - bh + 5, Palette::White);
        put_pixel(cx + 20, cy - bh + 5, Palette::White);
    }
}

void Renderer::draw_hud(float speed, float position, int lap) {
    // Simple text-free HUD bars and numbers via pixels (classic style)
    // Speed bar
    int bar_w = static_cast<int>((speed / 300.f) * 200.f);
    fill_rect(20, 20, 204, 16, Palette::Black);
    fill_rect(22, 22, bar_w, 12, Color{0x20, 0xC0, 0x20});
    // Lap indicator (dots)
    for (int i = 0; i < 3; ++i) {
        Color c = (i < lap) ? Palette::White : Color{0x40, 0x40, 0x40};
        fill_rect(20 + i * 20, 50, 12, 12, c);
    }
    (void)position;
}

} // namespace racer
