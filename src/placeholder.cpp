#include "placeholder.hpp"

#include <algorithm>
#include <cmath>

namespace racer::placeholder {

void draw_hud(Framebuffer& fb, float speed_fraction, int lap) {
    const int bar = static_cast<int>(std::clamp(speed_fraction, 0.f, 1.f) * 100.f);
    fb.fill_rect(8, 8, 104, 8, Palette::Black);
    fb.fill_rect(10, 10, bar, 4, Color{0x20, 0xc0, 0x20});
    for (int i = 0; i < std::min(lap, 10); ++i) {
        fb.fill_rect(8 + i * 8, 20, 5, 5, Palette::White);
    }
}

} // namespace racer::placeholder
