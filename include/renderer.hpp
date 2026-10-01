#pragma once
#include "types.hpp"
#include "components.hpp"
#include <cstdint>
#include <vector>
#include <cmath>

namespace racer {

class Renderer {
public:
    Renderer(int width, int height);

    void begin_frame();
    const uint32_t* pixels() const { return pixels_.data(); }

    // Classic scanline / polygon road drawing
    void draw_background(float sky_offset, float hill_offset);
    void draw_segment(const Projected& p1, const Projected& p2,
                      const Color& road, const Color& grass,
                      const Color& rumble, const Color& lane,
                      int lanes, float rumble_width);
    void draw_sprite(float screen_x, float screen_y, float scale,
                     int type, bool flip = false);
    void draw_player_car(float x, float y, float steer, float speed);
    void draw_hud(float speed, float position, int lap);

    int width() const { return w_; }
    int height() const { return h_; }

    // Direct pixel access for classic techniques
    void put_pixel(int x, int y, Color c);
    void fill_rect(int x, int y, int w, int h, Color c);
    void hline(int x1, int x2, int y, Color c);

private:
    int w_, h_;
    std::vector<uint32_t> pixels_;
};

} // namespace racer
