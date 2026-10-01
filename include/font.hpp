#pragma once
#include <cstdint>
#include <string_view>

namespace racer::font {

constexpr int glyph_w = 5;
constexpr int glyph_h = 7;
constexpr int advance = glyph_w + 1;

// Rows of a 5x7 glyph, bit 4 is the leftmost pixel. Lower case maps to
// upper case; unknown characters render as blanks.
const uint8_t* glyph(char c);

inline int text_width(std::string_view s, int scale = 1) {
    return s.empty() ? 0 : (static_cast<int>(s.size()) * advance - 1) * scale;
}

// Calls plot(x, y) for every set pixel of the text.
template <typename Plot>
void render(int x, int y, std::string_view s, int scale, Plot&& plot) {
    for (char ch : s) {
        const uint8_t* rows = glyph(ch);
        for (int gy = 0; gy < glyph_h; ++gy) {
            for (int gx = 0; gx < glyph_w; ++gx) {
                if (!(rows[gy] & (0x10 >> gx))) continue;
                for (int sy = 0; sy < scale; ++sy)
                    for (int sx = 0; sx < scale; ++sx)
                        plot(x + gx * scale + sx, y + gy * scale + sy);
            }
        }
        x += advance * scale;
    }
}

} // namespace racer::font
