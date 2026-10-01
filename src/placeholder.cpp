#include "placeholder.hpp"

#include <algorithm>
#include <cmath>

namespace racer::placeholder {

void draw_background(Framebuffer& fb, const RoadTheme& theme) {
    const int horizon = fb.height() / 2;
    for (int y = 0; y < horizon; ++y) {
        const float t = static_cast<float>(y) / static_cast<float>(horizon);
        fb.hline(0, fb.width(), y, blend(theme.sky_top, theme.sky_horizon, t));
    }
    fb.fill_rect(0, horizon, fb.width(), fb.height() - horizon, theme.grass[1]);
}

void draw_scenery(Framebuffer& fb, Scenery kind, float left, float bottom, float width, float fog) {
    const int x0 = pixel_edge(left);
    const int x1 = pixel_edge(left + width);
    const int w = x1 - x0;
    if (w < 1) return;
    const int yb = pixel_edge(bottom);
    const Color haze{0xc8, 0xe0, 0xe8};
    auto c = [&](Color col) { return blend(col, haze, 1.f - fog); };

    switch (kind) {
        case Scenery::Palm:
        case Scenery::Tree: {
            const int h = w * 2;
            fb.fill_rect(x0 + w * 2 / 5, yb - h, std::max(1, w / 5), h, c(Palette::TreeTrunk));
            for (int y = 0; y < h / 2; ++y) {
                const int hw = w * y / h;
                fb.hline(x0 + w / 2 - hw, x0 + w / 2 + hw + 1, yb - h + y, c(Palette::TreeGreen));
            }
            break;
        }
        case Scenery::Bush:
            fb.fill_rect(x0, yb - w / 2, w, w / 2, c(Palette::GrassDark));
            break;
        case Scenery::Boulder:
            fb.fill_rect(x0, yb - w * 2 / 3, w, w * 2 / 3, c(Palette::Cliff));
            break;
        case Scenery::Billboard:
            fb.fill_rect(x0, yb - w * 2 / 3, w, w / 2, c(Color{0xe0, 0xc0, 0x40}));
            break;
        case Scenery::Gantry:
            fb.fill_rect(x0, yb - w / 2, std::max(1, w / 30), w / 2, c(Palette::White));
            fb.fill_rect(x1 - std::max(1, w / 30), yb - w / 2, std::max(1, w / 30), w / 2, c(Palette::White));
            fb.fill_rect(x0, yb - w / 2, w, std::max(1, w / 12), c(Palette::CarRed));
            break;
    }
}

void draw_car(Framebuffer& fb, float center_x, float bottom, int steer) {
    const int cx = static_cast<int>(center_x);
    const int cy = static_cast<int>(bottom);
    const int lean = steer * 3;
    constexpr int bw = 72, bh = 22;
    fb.fill_rect(cx - bw / 2, cy - bh, bw, bh - 4, Palette::CarRed);
    fb.fill_rect(cx - bw / 3 + lean, cy - bh - 10, bw * 2 / 3, 10, Palette::CarDark);
    fb.fill_rect(cx - bw / 2 - 2, cy - 10, 10, 10, Palette::Black);
    fb.fill_rect(cx + bw / 2 - 8, cy - 10, 10, 10, Palette::Black);
}

void draw_hud(Framebuffer& fb, float speed_fraction, int lap) {
    const int bar = static_cast<int>(std::clamp(speed_fraction, 0.f, 1.f) * 100.f);
    fb.fill_rect(8, 8, 104, 8, Palette::Black);
    fb.fill_rect(10, 10, bar, 4, Color{0x20, 0xc0, 0x20});
    for (int i = 0; i < std::min(lap, 10); ++i) {
        fb.fill_rect(8 + i * 8, 20, 5, 5, Palette::White);
    }
}

} // namespace racer::placeholder
