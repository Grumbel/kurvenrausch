// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "background.hpp"
#include "canvas.hpp"
#include "daylight.hpp"
#include "road.hpp"
#include "sprites.hpp"
#include "track.hpp"
#include "weather.hpp"
#include "types.hpp"

#include <cstdint>
#include <memory>
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
    // Use other's uploaded bitmaps and atlas (same GL context).
    void share_textures(const GlesRenderer& other) { tc_ = other.tc_; }
    // How the backdrop is seen: zoom relative to the main view; a mirror shows
    // the panorama behind, flipped, and no sun or moon (BackdropView).
    void set_backdrop_view(float zoom, bool mirror) {
        backdrop_zoom_ = zoom;
        backdrop_mirror_ = mirror;
    }

    // Draws the road scene into the internal FBO (not the window): full-bright
    // albedo, then at night a lightmap evaluating the software street_lights /
    // headlight_beam formulas per pixel and a compose pass (apply_daylight).
    void render(const Track& track, const RoadView& view, const SpriteSheet& sprites,
                std::vector<RoadSprite>& objects, const RoadTheme& theme, const Daylight& light,
                const Background* backdrop = nullptr, float hour = 12.f,
                const Beam* headlight = nullptr, const Weather* weather = nullptr,
                const DrawList* overlay = nullptr);
    // Headless: `list` drawn over the presentable scene (into its FBO).
    void draw_over_scene(const DrawList& list);

    // Presentable scene texture (albedo, or night composite when applied).
    unsigned color_texture() const { return present_tex_ ? present_tex_ : color_tex_; }
    // A DrawList's runs of `layer` into the current framebuffer and viewport,
    // which map list.width() x list.height() framebuffer pixels; z is the
    // depth written where depth testing is on.
    void draw_list(const DrawList& list, int layer = 0, float z = 0.f);

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
    static double covered(const std::vector<Vertex>& tris);
    void ensure_font_tex();
    // Back to the scene program after a draw_list in the sprite pass.
    void use_scene_program();
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
    // Per-row headlight cone into rows_ lanes 2..4; its screen bounds into
    // box (x0, y0, x1, y1). False when no row is lit.
    bool headlight_rows(const Beam& beam, float dark, float box[4]);
    void draw_lamp_pools(float dark);
    void draw_weather(const Weather& weather);
    // Lightmap (lamps, headlight) and the night compose into night_tex_.
    void apply_night(const Daylight& light, const Beam* headlight);
    void read_fbo_argb(std::vector<uint32_t>& out);
    void draw_fullscreen_quad();
    void ensure_emissive_lut();
    // Depth written by the following sprite draws (flushes on change).
    void set_depth(float z);
    // rows_ texel (lane, row) = value as 24-bit fixed point.
    void set_row(int lane, int row, float value);
    // row_depth_ / row_center_x_ into lanes 0 / 1, then rows_ to rows_tex_.
    void upload_rows();
    // Light-program quad: a_uv = (u, v), a_col = (r, g, b, a), unclamped.
    void push_light_quad(float x0, float y0, float x1, float y1, float u, float v, float r, float g, float b,
                         float a);
    void flush_light();
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
    float backdrop_zoom_ = 1.f;
    bool backdrop_mirror_ = false;

    // Depth (NDC z) of the sprite pass and of the light quads tested against
    // it (GL_LESS against the depth buffer, cleared to 1 = bare ground).
    static constexpr float sprite_z = 0.f;
    static constexpr float player_z = -0.9f;
    static constexpr float lamp_z = 0.5f;  // lamps: bare ground only
    static constexpr float beam_z = -0.5f; // headlight: all but the player's car
    static constexpr int row_lanes = 8;
    float depth_z_ = sprite_z;

    unsigned program_ = 0;
    unsigned compose_program_ = 0;
    unsigned light_program_ = 0;
    unsigned ui_program_ = 0;
    unsigned font_tex_ = 0; // font_atlas::make()
    int u_ui_screen_ = -1;
    int u_ui_z_ = -1;
    int u_ui_tex_ = -1;
    std::vector<DrawList::Vertex> ui_batch_;
    unsigned vbo_ = 0;
    int u_screen_ = -1;
    int u_use_tex_ = -1;
    int u_tex_ = -1;
    int u_fog_air_ = -1;
    int u_z_ = -1;
    int u_albedo_ = -1;
    int u_light_ = -1;
    int u_emissive_ = -1;
    int u_day_scale_ = -1;
    int u_light_screen_ = -1;
    int u_light_z_ = -1;
    int u_light_rows_ = -1;
    int u_light_px_ = -1;
    int u_light_beam_ = -1;

    unsigned fbo_ = 0;
    unsigned color_tex_ = 0;   // albedo
    unsigned night_tex_ = 0;   // composed night
    unsigned present_tex_ = 0; // color_tex_ or night_tex_
    unsigned depth_rb_ = 0;
    unsigned light_fbo_ = 0;
    unsigned light_tex_ = 0;   // RGB: day colour share (tinted), A: night colour share
    unsigned night_fbo_ = 0;
    unsigned emissive_tex_ = 0; // (red, green) → emissive blues, see k_compose_frag
    unsigned rows_tex_ = 0;     // per-row lanes for the light program (rows_)
    int rows_tex_h_ = 0;        // rows (texture width) rows_tex_ was allocated for
    std::vector<uint8_t> rows_;
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
    // Uploaded bitmaps, shared by renderers drawing in the same GL context
    // (the main view and the mirror) so the atlas exists once.
    struct TextureCache {
        std::unordered_map<const uint32_t*, CachedTex> textures;
        // One or more atlas pages (scenery + static vehicles). atlas_tex is
        // page 0 (the white tint texel lives there); textures point at
        // whichever page holds them.
        std::vector<unsigned> atlas_pages;
        unsigned atlas_tex = 0; // == atlas_pages[0] when non-empty
        int atlas_w = 0, atlas_h = 0; // size of each page (power of two)
        bool atlas_ready = false;
        // 1×1 white texel in the atlas — solid-coloured quads share the sprite batch.
        float white_u = 0.f, white_v = 0.f;
    };
    std::shared_ptr<TextureCache> tc_ = std::make_shared<TextureCache>();
};

} // namespace racer
