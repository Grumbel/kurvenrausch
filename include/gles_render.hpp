// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "road.hpp"
#include "sprites.hpp"
#include "track.hpp"
#include "types.hpp"

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace racer {

class Display;

// GLES2 / desktop-GL scene renderer for the road view. Sprite bitmaps stay
// CPU-authored; once uploaded they are drawn as textured quads. Road surfaces
// are solid-colour trapezoids. Uses the GL context owned by Display.
class GlesRenderer {
public:
    GlesRenderer() = default;
    ~GlesRenderer();
    GlesRenderer(const GlesRenderer&) = delete;
    GlesRenderer& operator=(const GlesRenderer&) = delete;

    // Compiles shaders and creates the VBO. Display must have a current GL
    // context (PresentBackend::Gl). Returns false on failure.
    bool init();
    void shutdown();
    bool ready() const { return program_ != 0; }

    // Internal resolution (same as the former software framebuffer).
    void set_size(int width, int height);

    // Clear, draw road + sprites. `daylight` multiplies RGB (1 = full day).
    void render(const Track& track, const RoadView& view, const SpriteSheet& sprites,
                std::vector<RoadSprite>& objects, const RoadTheme& theme, float daylight = 1.f,
                int viewport_w = 0, int viewport_h = 0);

    // After render: depth per row and lamp spots (for optional CPU effects).
    const std::vector<float>& row_depth() const { return row_depth_; }
    const std::vector<LampSpot>& lamps() const { return lamps_; }

private:
    struct Vertex {
        float x, y, u, v;
        float r, g, b, a;
    };
    struct Slice {
        int index = 0;
        ScreenPoint p1, p2;
        float clip = 0.f;
        float top = 0.f;
        float left = 0.f, right = 0.f;
        float fog = 1.f;
        bool road_visible = false;
    };

    void clear_batch();
    void push_trap(float y0, float x0l, float x0r, float y1, float x1l, float x1r, Color c);
    void push_quad(float x, float y, float w, float h, float u0, float v0, float u1, float v1, Color c, bool flip);
    void flush_solid();
    void flush_textured(unsigned tex);
    unsigned texture_for(const Bitmap& bmp);
    void draw_segment(const Track& track, const Slice& s, const RoadTheme& theme);
    void draw_sprites(const Track& track, const SpriteSheet& sprites, std::vector<RoadSprite>& objects);

    void project_point(ScreenPoint& p, float world_x, float world_y, float world_z, float cam_x, float cam_y,
                       float cam_z, float depth, int direction, int screen_w, float x_scale, float horizon,
                       float y_scale, float road_width) const;

    int width_ = 320;
    int height_ = 240;
    float x_scale_ = 1.f;
    float y_scale_ = 1.f;
    float camera_depth_ = 1.f;
    int direction_ = 1;
    Color fog_air_{};
    float window_wake_ = 0.f;
    float daylight_ = 1.f;

    unsigned program_ = 0;
    unsigned vbo_ = 0;
    int u_screen_ = -1;
    int u_use_tex_ = -1;
    int u_tex_ = -1;

    std::vector<Vertex> solid_;
    std::vector<Vertex> textured_;
    std::vector<Slice> slices_;
    std::vector<float> row_depth_;
    std::vector<LampSpot> lamps_;
    std::unordered_map<const uint32_t*, unsigned> textures_;
};

} // namespace racer
