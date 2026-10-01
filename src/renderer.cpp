#include "renderer.hpp"
#include <algorithm>
#include <cstring>
#include <cmath>

namespace racer {

Renderer::Renderer(int width, int height) : w_(width), h_(height) {
    pixels_.resize(static_cast<size_t>(w_ * h_), 0);
}

Renderer::~Renderer() {
    if (texture_) SDL_DestroyTexture(texture_);
    if (format_) SDL_FreeFormat(format_);
    // sdl_renderer_ is owned by us if we created it
    if (sdl_renderer_) SDL_DestroyRenderer(sdl_renderer_);
}

bool Renderer::init(SDL_Window* window) {
    sdl_renderer_ = SDL_CreateRenderer(window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!sdl_renderer_) {
        // fallback
        sdl_renderer_ = SDL_CreateRenderer(window, -1, 0);
    }
    if (!sdl_renderer_) return false;

    // ARGB8888 is the most portable "AARRGGBB in a uint32" on desktop
    texture_ = SDL_CreateTexture(sdl_renderer_, SDL_PIXELFORMAT_ARGB8888,
                                 SDL_TEXTUREACCESS_STREAMING, w_, h_);
    if (!texture_) return false;

    format_ = SDL_AllocFormat(SDL_PIXELFORMAT_ARGB8888);
    return true;
}

void Renderer::begin_frame() {
    std::fill(pixels_.begin(), pixels_.end(), 0u);
}

void Renderer::end_frame() {
    void* tex_pixels = nullptr;
    int pitch = 0;
    if (SDL_LockTexture(texture_, nullptr, &tex_pixels, &pitch) == 0) {
        uint8_t* dst = static_cast<uint8_t*>(tex_pixels);
        const uint32_t* src = pixels_.data();
        for (int y = 0; y < h_; ++y) {
            std::memcpy(dst + y * pitch, src + y * w_,
                        static_cast<size_t>(w_) * sizeof(uint32_t));
        }
        SDL_UnlockTexture(texture_);
    }
    SDL_RenderClear(sdl_renderer_);
    SDL_RenderCopy(sdl_renderer_, texture_, nullptr, nullptr);
    SDL_RenderPresent(sdl_renderer_);
}

void Renderer::put_pixel(int x, int y, Color c) {
    if (x < 0 || x >= w_ || y < 0 || y >= h_) return;
    pixels_[static_cast<size_t>(y * w_ + x)] = c.to_u32();
}

void Renderer::hline(int x1, int x2, int y, Color c) {
    if (y < 0 || y >= h_) return;
    if (x1 > x2) std::swap(x1, x2);
    x1 = std::max(0, x1);
    x2 = std::min(w_ - 1, x2);
    if (x1 > x2) return;
    uint32_t col = c.to_u32();
    uint32_t* row = &pixels_[static_cast<size_t>(y * w_)];
    for (int x = x1; x <= x2; ++x) row[x] = col;
}

void Renderer::fill_rect(int x, int y, int ww, int hh, Color c) {
    for (int j = 0; j < hh; ++j)
        hline(x, x + ww - 1, y + j, c);
}

void Renderer::draw_background(float /*sky_offset*/, float /*hill_offset*/) {
    // Smooth sky gradient (top → horizon)
    int horizon = h_ * 55 / 100;  // slightly below center looks better
    for (int y = 0; y < horizon; ++y) {
        float t = static_cast<float>(y) / static_cast<float>(horizon);
        Color sky{
            static_cast<uint8_t>(lerp(Palette::SkyTop.r, Palette::SkyBottom.r, t)),
            static_cast<uint8_t>(lerp(Palette::SkyTop.g, Palette::SkyBottom.g, t)),
            static_cast<uint8_t>(lerp(Palette::SkyTop.b, Palette::SkyBottom.b, t))
        };
        hline(0, w_ - 1, y, sky);
    }
    // Fill rest with dark (will be overdrawn by road/grass)
    for (int y = horizon; y < h_; ++y)
        hline(0, w_ - 1, y, Palette::GrassDark);
}

// Draw a trapezoid between two projected points by scanline interpolation
static void fill_trapezoid(Renderer& r, float x1, float y1, float w1,
                           float x2, float y2, float w2, Color c) {
    int iy1 = static_cast<int>(std::round(y1));
    int iy2 = static_cast<int>(std::round(y2));
    if (iy1 == iy2) {
        r.hline(static_cast<int>(x1 - w1), static_cast<int>(x1 + w1), iy1, c);
        return;
    }
    if (iy1 > iy2) {
        std::swap(iy1, iy2);
        std::swap(x1, x2);
        std::swap(w1, w2);
    }
    float dy = static_cast<float>(iy2 - iy1);
    for (int y = iy1; y <= iy2; ++y) {
        float t = (y - iy1) / dy;
        float x  = lerp(x1, x2, t);
        float ww = lerp(w1, w2, t);
        r.hline(static_cast<int>(x - ww), static_cast<int>(x + ww), y, c);
    }
}

void Renderer::draw_segment(const Projected& p1, const Projected& p2,
                            const Color& road, const Color& grass,
                            const Color& rumble, const Color& lane,
                            int lanes, float rumble_width) {
    // Full-width grass
    fill_trapezoid(*this, w_ / 2.f, p1.y, w_ / 2.f,
                          w_ / 2.f, p2.y, w_ / 2.f, grass);

    // Rumble strips (outside the road)
    float r1 = p1.w * rumble_width;
    float r2 = p2.w * rumble_width;
    fill_trapezoid(*this, p1.x - p1.w - r1 * 0.5f, p1.y, r1 * 0.5f,
                          p2.x - p2.w - r2 * 0.5f, p2.y, r2 * 0.5f, rumble);
    fill_trapezoid(*this, p1.x + p1.w + r1 * 0.5f, p1.y, r1 * 0.5f,
                          p2.x + p2.w + r2 * 0.5f, p2.y, r2 * 0.5f, rumble);

    // Road surface
    fill_trapezoid(*this, p1.x, p1.y, p1.w, p2.x, p2.y, p2.w, road);

    // Lane markers
    if (lanes > 1) {
        float lw1 = std::max(1.f, p1.w * 0.025f);
        float lw2 = std::max(1.f, p2.w * 0.025f);
        for (int i = 1; i < lanes; ++i) {
            float frac = static_cast<float>(i) / static_cast<float>(lanes);
            float lx1 = p1.x - p1.w + 2.f * p1.w * frac;
            float lx2 = p2.x - p2.w + 2.f * p2.w * frac;
            fill_trapezoid(*this, lx1, p1.y, lw1, lx2, p2.y, lw2, lane);
        }
    }
}

void Renderer::draw_sprite(float screen_x, float screen_y, float scale,
                           int type, bool flip) {
    int sw = static_cast<int>(std::round(80.f * scale));
    int sh = static_cast<int>(std::round(120.f * scale));
    if (sw < 2 || sh < 2) return;

    int sx = static_cast<int>(std::round(screen_x)) - sw / 2;
    int sy = static_cast<int>(std::round(screen_y)) - sh;

    if (type == 0) {
        // Tree: trunk + layered foliage
        int tw = std::max(2, sw / 6);
        fill_rect(sx + sw / 2 - tw / 2, sy + sh * 2 / 5, tw, sh * 3 / 5, Palette::TreeTrunk);
        for (int layer = 0; layer < 3; ++layer) {
            int top = sy + layer * sh / 6;
            int bot = sy + sh * 2 / 5 + layer * sh / 10;
            for (int y = top; y < bot; ++y) {
                float t = 1.f - static_cast<float>(y - top) / std::max(1, bot - top);
                int hw = static_cast<int>((sw / 2) * (0.3f + 0.7f * t));
                Color leaf = (layer % 2 == 0) ? Palette::TreeGreen : Palette::TreeDark;
                hline(sx + sw / 2 - hw, sx + sw / 2 + hw, y, leaf);
            }
        }
    } else if (type == 1) {
        // Cliff / rock face
        for (int y = 0; y < sh; ++y) {
            float t = static_cast<float>(y) / sh;
            int hw = static_cast<int>(sw * (0.35f + 0.65f * t));
            Color c{
                static_cast<uint8_t>(Palette::Cliff.r + (y % 7) * 2),
                static_cast<uint8_t>(Palette::Cliff.g + (y % 5)),
                static_cast<uint8_t>(Palette::Cliff.b)
            };
            int cx = flip ? (sx + sw - hw) : sx;
            hline(cx, cx + hw, sy + y, c);
        }
    } else {
        // Billboard
        fill_rect(sx, sy, sw, sh, Color{0x90, 0x90, 0x70});
        fill_rect(sx + 3, sy + 3, sw - 6, sh - 6, Color{0xB0, 0xB0, 0x90});
        // simple "logo" bar
        fill_rect(sx + sw / 4, sy + sh / 3, sw / 2, sh / 8, Palette::CarBody);
    }
}

void Renderer::draw_player_car(float x, float y, float steer, float /*speed*/) {
    int cx = static_cast<int>(std::round(x));
    int cy = static_cast<int>(std::round(y));
    int lean = static_cast<int>(steer * 10.f);

    // Shadow
    fill_rect(cx - 42, cy - 6, 84, 10, Color{0x20, 0x20, 0x20, 180});

    // Body
    int bw = 70, bh = 28;
    fill_rect(cx - bw / 2 + lean, cy - bh, bw, bh, Palette::CarBody);
    // Roof / cabin
    fill_rect(cx - bw / 3 + lean / 2, cy - bh - 16, bw * 2 / 3, 18, Palette::CarDark);
    // Windscreen
    fill_rect(cx - bw / 4 + lean / 2, cy - bh - 12, bw / 2, 10, Palette::CarGlass);
    // Wheels
    fill_rect(cx - bw / 2 - 2 + lean, cy - 10, 14, 14, Palette::Black);
    fill_rect(cx + bw / 2 - 12 + lean, cy - 10, 14, 14, Palette::Black);
    // Headlight dots
    put_pixel(cx - 18 + lean, cy - bh + 4, Palette::White);
    put_pixel(cx + 18 + lean, cy - bh + 4, Palette::White);
}

void Renderer::draw_hud(float speed, float /*position*/, int lap) {
    // Speed bar background
    fill_rect(16, 16, 208, 18, Palette::Black);
    int bar = static_cast<int>((speed / 300.f) * 200.f);
    bar = std::max(0, std::min(200, bar));
    Color bar_col = speed > 250.f ? Color{0xE0, 0x40, 0x20} : Color{0x30, 0xC0, 0x30};
    fill_rect(18, 18, bar, 14, bar_col);

    // Lap dots
    for (int i = 0; i < 5; ++i) {
        Color c = (i < lap) ? Palette::White : Color{0x50, 0x50, 0x50};
        fill_rect(16 + i * 18, 42, 12, 12, c);
    }
}

} // namespace racer
