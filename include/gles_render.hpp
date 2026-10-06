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
    // Full-bright albedo → ground ReadPixels → sprites → CPU night
    // (apply_daylight / street_lights / headlight_beam) → upload. Same math
    // as the software path so pools and cones stay smooth.
    void render(const Track& track, const RoadView& view, const SpriteSheet& sprites,
                std::vector<RoadSprite>& objects, const RoadTheme& theme, const Daylight& light,
                const Background* backdrop = nullptr, float hour = 12.f,
                const Beam* headlight = nullptr, const Weather* weather = nullptr);

    // Presentable scene texture (albedo, or night composite when applied).
    unsigned color_texture() const { return present_tex_ ? present_tex_ : color_tex_; }
    // The presentable scene as ARGB pixels, top row first (headless screenshots).
    void read_scene_argb(std::vector<uint32_t>& out);
    int texture_width() const { return width_; }
    int texture_height() const { return height_; }

    const std::vector<float>& row_depth() const { return row_depth_; }
    const std::vector<float>& row_center_x() const { return row_center_x_; }
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
    void set_draw_clip(int x0, int y0, int x1, int y1);
    void clear_draw_clip();
    void push_trap(float y0, float x0l, float x0r, float y1, float x1l, float x1r, Color c);
    // Smooth gradient: colour c0 on the y0 edge, c1 on the y1 edge (GPU interpolates).
    void push_trap_vcol(float y0, float x0l, float x0r, float y1, float x1l, float x1r, Color c0, Color c1);
    void push_solid_quad(float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3,
                         Color c);
    // Solid-looking quad via atlas white texel + vertex colour (same batch as sprites).
    void push_tint_quad(float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3,
                        Color c);
    void push_tex_quad(float x0, float y0, float u0, float v0, float x1, float y1, float u1, float v1,
                       float x2, float y2, float u2, float v2, float x3, float y3, float u3, float v3, Color c);
    void push_quad(float x, float y, float w, float h, float u0, float v0, float u1, float v1, Color c, bool flip);
    void push_quad_rotated(float cx, float cy, float w, float h, float angle, float u0, float v0, float u1,
                           float v1, Color c);
    void flush_solid();
    // Draw pending textured_ with active_tex_, then clear the batch.
    void flush_textured();
    // Bind tex for subsequent push_quad* into textured_. Flushes if the
    // active texture changes so consecutive same-tex quads share one draw.
    void set_textured(unsigned tex);
    // GPU texture + UV rect (atlas or whole texture). dynamic=true re-uploads
    // pixels each call (player composite).
    struct TexRef {
        unsigned id = 0;
        float u0 = 0.f, v0 = 0.f, u1 = 1.f, v1 = 1.f;
        explicit operator bool() const { return id != 0; }
    };
    TexRef texture_for(const Bitmap& bmp, bool dynamic = false);
    // Pack static SpriteSheet bitmaps into one atlas (idempotent).
    void ensure_sprite_atlas(const SpriteSheet& sprites);
    void draw_backdrop(const RoadTheme& theme, const Background* backdrop, float hour, float horizon);
    void draw_headlight(const Beam& beam, float dark);
    void draw_lamp_pools(float dark);
    void draw_weather(const Weather& weather);
    // GPU lightmap path kept for reference; night uses apply_cpu_night.
    void apply_gpu_night(const Daylight& light, const Beam* headlight);
    // Pixel-perfect night: software light stack on ReadPixels albedo.
    void apply_cpu_night(const Daylight& light, const Beam* headlight,
                         const std::vector<uint32_t>& ground_argb,
                         const std::vector<uint32_t>& day_argb);
    void read_fbo_argb(std::vector<uint32_t>& out);
    void draw_fullscreen_quad();
    void copy_tex_to_fbo(unsigned src_tex, unsigned dst_fbo);
    void ensure_emissive_lut();
    void ensure_falloff_tex();
    void ensure_beam_falloff_tex();
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
    unsigned compose_program_ = 0;
    unsigned vbo_ = 0;
    int u_screen_ = -1;
    int u_use_tex_ = -1;
    int u_tex_ = -1;
    int u_fog_air_ = -1;
    int u_compose_screen_ = -1;
    int u_albedo_ = -1;
    int u_light_ = -1;
    int u_ground_ = -1;
    int u_emissive_ = -1;
    int u_day_scale_ = -1;
    int u_emissive_count_ = -1;
    int u_copy_tex_ = -1;

    unsigned fbo_ = 0;
    unsigned color_tex_ = 0;   // albedo
    unsigned night_tex_ = 0;   // composed night
    unsigned present_tex_ = 0; // color_tex_ or night_tex_
    unsigned depth_rb_ = 0;
    unsigned light_fbo_ = 0;
    unsigned light_tex_ = 0;   // per-channel mix-to-day factors
    unsigned night_fbo_ = 0;
    unsigned ground_fbo_ = 0;
    unsigned ground_tex_ = 0;  // pre-sprite road/backdrop copy
    unsigned copy_program_ = 0;
    unsigned emissive_tex_ = 0;
    unsigned falloff_tex_ = 0; // soft radial (1-r^2)^2 for lamp pools
    unsigned beam_falloff_tex_ = 0; // 1D lateral headlight falloff (software shape)
    int emissive_count_ = 0;
    int fbo_w_ = 0, fbo_h_ = 0;

    std::vector<Vertex> solid_;
    std::vector<Vertex> textured_;
    unsigned active_tex_ = 0; // GL texture for the current textured_ batch
    std::vector<Slice> slices_;
    std::vector<float> row_depth_;
    std::vector<float> row_center_x_;
    std::vector<LampSpot> lamps_;
    // CPU clip for solid/textured pushes (avoids GL scissor flushes per slice).
    int draw_clip_x0_ = 0, draw_clip_y0_ = 0, draw_clip_x1_ = 0, draw_clip_y1_ = 0;
    bool draw_clip_ = false;
    struct CachedTex {
        unsigned id = 0;
        int w = 0;
        int h = 0;
        float u0 = 0.f, v0 = 0.f, u1 = 1.f, v1 = 1.f;
        bool in_atlas = false;
    };
    std::unordered_map<const uint32_t*, CachedTex> textures_;
    // One or more atlas pages (scenery + static vehicles). atlas_tex_ is page 0
    // (white tint texel lives there); textures_ point at whichever page holds them.
    std::vector<unsigned> atlas_pages_;
    unsigned atlas_tex_ = 0; // == atlas_pages_[0] when non-empty
    int atlas_w_ = 0, atlas_h_ = 0; // size of each page (power of two)
    bool atlas_ready_ = false;
    // 1×1 white texel in the atlas — solid-coloured quads share the sprite batch.
    float white_u_ = 0.f, white_v_ = 0.f;
};

} // namespace racer
