// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "background.hpp"
#include "daylight.hpp"
#include "road.hpp"
#include "sprites.hpp"
#include "track.hpp"
#include "weather.hpp"
#include "types.hpp"

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace racer {

// GLES2 / desktop-GL scene renderer for the road view. Sprite bitmaps stay
// CPU-authored; once uploaded they are drawn as textured quads. Road surfaces
// are solid-colour trapezoids. Renders into an FBO at internal resolution;
// the colour texture is presented letterboxed by Display.
class GlesRenderer {
public:
    GlesRenderer() = default;
    ~GlesRenderer();
    GlesRenderer(const GlesRenderer&) = delete;
    GlesRenderer& operator=(const GlesRenderer&) = delete;

    bool init();
    // Drop GL object names without calling GL (context was destroyed).
    void invalidate();
    void shutdown();
    bool ready() const { return program_ != 0; }

    void set_size(int width, int height);

    // Draws the road scene into the internal FBO (not the window).
    // `light` drives CPU night (apply_daylight / street_lights / headlight_beam)
    // after the full-bright GPU albedo pass — same math as the software path.
    void render(const Track& track, const RoadView& view, const SpriteSheet& sprites,
                std::vector<RoadSprite>& objects, const RoadTheme& theme, const Daylight& light,
                const Background* backdrop = nullptr, float hour = 12.f,
                const Beam* headlight = nullptr, const Weather* weather = nullptr);

    // Colour attachment of the scene FBO (RGBA, size width_ × height_).
    unsigned color_texture() const { return color_tex_; }
    int texture_width() const { return width_; }
    int texture_height() const { return height_; }

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

    bool ensure_fbo();
    void clear_batch();
    void push_trap(float y0, float x0l, float x0r, float y1, float x1l, float x1r, Color c);
    // Smooth gradient: colour c0 on the y0 edge, c1 on the y1 edge (GPU interpolates).
    void push_trap_vcol(float y0, float x0l, float x0r, float y1, float x1l, float x1r, Color c0, Color c1);
    void push_solid_quad(float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3,
                         Color c);
    void push_quad(float x, float y, float w, float h, float u0, float v0, float u1, float v1, Color c, bool flip);
    void push_quad_rotated(float cx, float cy, float w, float h, float angle, float u0, float v0, float u1,
                           float v1, Color c);
    void flush_solid();
    void flush_textured(unsigned tex);
    // Immutable scenery: cache by px.data(). dynamic=true re-uploads pixels
    // each call (player_bitmap_ is composited into a reused buffer).
    unsigned texture_for(const Bitmap& bmp, bool dynamic = false);
    void draw_backdrop(const RoadTheme& theme, const Background* backdrop, float hour, float horizon);
    void draw_headlight(const Beam& beam, float dark);
    void draw_lamp_pools(float dark);
    void draw_weather(const Weather& weather);
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
    int u_fog_air_ = -1;

    unsigned fbo_ = 0;
    unsigned color_tex_ = 0;
    unsigned depth_rb_ = 0;
    unsigned light_fbo_ = 0;
    unsigned light_tex_ = 0;
    int fbo_w_ = 0, fbo_h_ = 0;

    std::vector<Vertex> solid_;
    std::vector<Vertex> textured_;
    std::vector<Slice> slices_;
    std::vector<float> row_depth_;
    std::vector<LampSpot> lamps_;
    struct CachedTex {
        unsigned id = 0;
        int w = 0;
        int h = 0;
    };
    std::unordered_map<const uint32_t*, CachedTex> textures_;
};

} // namespace racer
