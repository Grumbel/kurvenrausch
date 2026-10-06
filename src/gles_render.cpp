// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "gles_render.hpp"
#include "framebuffer.hpp"

#include "daylight.hpp"

#include <SDL2/SDL.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>

// Use the same GL entry points as display.cpp via SDL_GL_GetProcAddress.
namespace racer {
namespace {

#if defined(__ANDROID__) || defined(__EMSCRIPTEN__)
#define KURVEN_GLES 1
#else
#define KURVEN_GLES 0
#endif

constexpr unsigned GL_COLOR_BUFFER_BIT_ = 0x00004000;
constexpr unsigned GL_BLEND_ = 0x0BE2;
constexpr unsigned GL_SRC_ALPHA_ = 0x0302;
constexpr unsigned GL_ONE_MINUS_SRC_ALPHA_ = 0x0303;
constexpr unsigned GL_TEXTURE_2D_ = 0x0DE1;
constexpr unsigned GL_TEXTURE0_ = 0x84C0;
constexpr unsigned GL_TRIANGLE_STRIP_ = 0x0005;
constexpr unsigned GL_TRIANGLES_ = 0x0004;
constexpr unsigned GL_ARRAY_BUFFER_ = 0x8892;
constexpr unsigned GL_DYNAMIC_DRAW_ = 0x88E8;
constexpr unsigned GL_FLOAT_ = 0x1406;
constexpr unsigned GL_FALSE_ = 0;
constexpr unsigned GL_VERTEX_SHADER_ = 0x8B31;
constexpr unsigned GL_FRAGMENT_SHADER_ = 0x8B30;
constexpr unsigned GL_COMPILE_STATUS_ = 0x8B81;
constexpr unsigned GL_LINK_STATUS_ = 0x8B82;
constexpr unsigned GL_RGBA_ = 0x1908;
constexpr unsigned GL_UNSIGNED_BYTE_ = 0x1401;
constexpr unsigned GL_TEXTURE_MIN_FILTER_ = 0x2801;
constexpr unsigned GL_TEXTURE_MAG_FILTER_ = 0x2800;
constexpr unsigned GL_NEAREST_ = 0x2600;
constexpr unsigned GL_TEXTURE_WRAP_S_ = 0x2802;
constexpr unsigned GL_TEXTURE_WRAP_T_ = 0x2803;
constexpr unsigned GL_CLAMP_TO_EDGE_ = 0x812F;
constexpr unsigned GL_SCISSOR_TEST_ = 0x0C11;

struct GlApi {
    void (*ClearColor)(float, float, float, float) = nullptr;
    void (*Clear)(unsigned) = nullptr;
    void (*Viewport)(int, int, int, int) = nullptr;
    void (*Enable)(unsigned) = nullptr;
    void (*Disable)(unsigned) = nullptr;
    void (*BlendFunc)(unsigned, unsigned) = nullptr;
    void (*Scissor)(int, int, int, int) = nullptr;
    unsigned (*CreateShader)(unsigned) = nullptr;
    void (*ShaderSource)(unsigned, int, const char* const*, const int*) = nullptr;
    void (*CompileShader)(unsigned) = nullptr;
    void (*GetShaderiv)(unsigned, unsigned, int*) = nullptr;
    void (*GetShaderInfoLog)(unsigned, int, int*, char*) = nullptr;
    void (*DeleteShader)(unsigned) = nullptr;
    unsigned (*CreateProgram)() = nullptr;
    void (*AttachShader)(unsigned, unsigned) = nullptr;
    void (*BindAttribLocation)(unsigned, unsigned, const char*) = nullptr;
    void (*LinkProgram)(unsigned) = nullptr;
    void (*GetProgramiv)(unsigned, unsigned, int*) = nullptr;
    void (*GetProgramInfoLog)(unsigned, int, int*, char*) = nullptr;
    void (*DeleteProgram)(unsigned) = nullptr;
    void (*UseProgram)(unsigned) = nullptr;
    int (*GetUniformLocation)(unsigned, const char*) = nullptr;
    void (*Uniform1i)(int, int) = nullptr;
    void (*Uniform2f)(int, float, float) = nullptr;
    void (*GenBuffers)(int, unsigned*) = nullptr;
    void (*DeleteBuffers)(int, const unsigned*) = nullptr;
    void (*BindBuffer)(unsigned, unsigned) = nullptr;
    void (*BufferData)(unsigned, std::ptrdiff_t, const void*, unsigned) = nullptr;
    void (*EnableVertexAttribArray)(unsigned) = nullptr;
    void (*DisableVertexAttribArray)(unsigned) = nullptr;
    void (*VertexAttribPointer)(unsigned, int, unsigned, unsigned char, int, const void*) = nullptr;
    void (*DrawArrays)(unsigned, int, int) = nullptr;
    void (*GenTextures)(int, unsigned*) = nullptr;
    void (*DeleteTextures)(int, const unsigned*) = nullptr;
    void (*BindTexture)(unsigned, unsigned) = nullptr;
    void (*ActiveTexture)(unsigned) = nullptr;
    void (*TexImage2D)(unsigned, int, int, int, int, int, unsigned, unsigned, const void*) = nullptr;
    void (*TexParameteri)(unsigned, unsigned, int) = nullptr;
    void (*PixelStorei)(unsigned, int) = nullptr;
} g;

template <typename T>
T load(const char* name) {
    return reinterpret_cast<T>(SDL_GL_GetProcAddress(name));
}

bool load_gl() {
    if (g.Clear) return true;
    g.ClearColor = load<decltype(g.ClearColor)>("glClearColor");
    g.Clear = load<decltype(g.Clear)>("glClear");
    g.Viewport = load<decltype(g.Viewport)>("glViewport");
    g.Enable = load<decltype(g.Enable)>("glEnable");
    g.Disable = load<decltype(g.Disable)>("glDisable");
    g.BlendFunc = load<decltype(g.BlendFunc)>("glBlendFunc");
    g.Scissor = load<decltype(g.Scissor)>("glScissor");
    g.CreateShader = load<decltype(g.CreateShader)>("glCreateShader");
    g.ShaderSource = load<decltype(g.ShaderSource)>("glShaderSource");
    g.CompileShader = load<decltype(g.CompileShader)>("glCompileShader");
    g.GetShaderiv = load<decltype(g.GetShaderiv)>("glGetShaderiv");
    g.GetShaderInfoLog = load<decltype(g.GetShaderInfoLog)>("glGetShaderInfoLog");
    g.DeleteShader = load<decltype(g.DeleteShader)>("glDeleteShader");
    g.CreateProgram = load<decltype(g.CreateProgram)>("glCreateProgram");
    g.AttachShader = load<decltype(g.AttachShader)>("glAttachShader");
    g.BindAttribLocation = load<decltype(g.BindAttribLocation)>("glBindAttribLocation");
    g.LinkProgram = load<decltype(g.LinkProgram)>("glLinkProgram");
    g.GetProgramiv = load<decltype(g.GetProgramiv)>("glGetProgramiv");
    g.GetProgramInfoLog = load<decltype(g.GetProgramInfoLog)>("glGetProgramInfoLog");
    g.DeleteProgram = load<decltype(g.DeleteProgram)>("glDeleteProgram");
    g.UseProgram = load<decltype(g.UseProgram)>("glUseProgram");
    g.GetUniformLocation = load<decltype(g.GetUniformLocation)>("glGetUniformLocation");
    g.Uniform1i = load<decltype(g.Uniform1i)>("glUniform1i");
    g.Uniform2f = load<decltype(g.Uniform2f)>("glUniform2f");
    g.GenBuffers = load<decltype(g.GenBuffers)>("glGenBuffers");
    g.DeleteBuffers = load<decltype(g.DeleteBuffers)>("glDeleteBuffers");
    g.BindBuffer = load<decltype(g.BindBuffer)>("glBindBuffer");
    g.BufferData = load<decltype(g.BufferData)>("glBufferData");
    g.EnableVertexAttribArray = load<decltype(g.EnableVertexAttribArray)>("glEnableVertexAttribArray");
    g.DisableVertexAttribArray = load<decltype(g.DisableVertexAttribArray)>("glDisableVertexAttribArray");
    g.VertexAttribPointer = load<decltype(g.VertexAttribPointer)>("glVertexAttribPointer");
    g.DrawArrays = load<decltype(g.DrawArrays)>("glDrawArrays");
    g.GenTextures = load<decltype(g.GenTextures)>("glGenTextures");
    g.DeleteTextures = load<decltype(g.DeleteTextures)>("glDeleteTextures");
    g.BindTexture = load<decltype(g.BindTexture)>("glBindTexture");
    g.ActiveTexture = load<decltype(g.ActiveTexture)>("glActiveTexture");
    g.TexImage2D = load<decltype(g.TexImage2D)>("glTexImage2D");
    g.TexParameteri = load<decltype(g.TexParameteri)>("glTexParameteri");
    g.PixelStorei = load<decltype(g.PixelStorei)>("glPixelStorei");
    return g.Clear && g.CreateShader && g.DrawArrays && g.TexImage2D;
}

const char* k_vert =
#if KURVEN_GLES
    "attribute vec2 a_pos;\n"
    "attribute vec2 a_uv;\n"
    "attribute vec4 a_col;\n"
    "uniform vec2 u_screen;\n"
    "varying vec2 v_uv;\n"
    "varying vec4 v_col;\n"
    "void main(){\n"
    "  v_uv = a_uv; v_col = a_col;\n"
    "  vec2 ndc = vec2(a_pos.x / u_screen.x * 2.0 - 1.0, 1.0 - a_pos.y / u_screen.y * 2.0);\n"
    "  gl_Position = vec4(ndc, 0.0, 1.0);\n"
    "}\n";
#else
    "#version 110\n"
    "attribute vec2 a_pos;\n"
    "attribute vec2 a_uv;\n"
    "attribute vec4 a_col;\n"
    "uniform vec2 u_screen;\n"
    "varying vec2 v_uv;\n"
    "varying vec4 v_col;\n"
    "void main(){\n"
    "  v_uv = a_uv; v_col = a_col;\n"
    "  vec2 ndc = vec2(a_pos.x / u_screen.x * 2.0 - 1.0, 1.0 - a_pos.y / u_screen.y * 2.0);\n"
    "  gl_Position = vec4(ndc, 0.0, 1.0);\n"
    "}\n";
#endif

const char* k_frag =
#if KURVEN_GLES
    "precision mediump float;\n"
    "varying vec2 v_uv;\n"
    "varying vec4 v_col;\n"
    "uniform sampler2D u_tex;\n"
    "uniform int u_use_tex;\n"
    "void main(){\n"
    "  vec4 t = u_use_tex != 0 ? texture2D(u_tex, v_uv) : vec4(1.0);\n"
    "  if (u_use_tex != 0 && t.a < 0.01) discard;\n"
    "  gl_FragColor = t * v_col;\n"
    "}\n";
#else
    "#version 110\n"
    "varying vec2 v_uv;\n"
    "varying vec4 v_col;\n"
    "uniform sampler2D u_tex;\n"
    "uniform int u_use_tex;\n"
    "void main(){\n"
    "  vec4 t = u_use_tex != 0 ? texture2D(u_tex, v_uv) : vec4(1.0);\n"
    "  if (u_use_tex != 0 && t.a < 0.01) discard;\n"
    "  gl_FragColor = t * v_col;\n"
    "}\n";
#endif

unsigned compile(unsigned type, const char* src) {
    const unsigned s = g.CreateShader(type);
    g.ShaderSource(s, 1, &src, nullptr);
    g.CompileShader(s);
    int ok = 0;
    g.GetShaderiv(s, GL_COMPILE_STATUS_, &ok);
    if (!ok) {
        char log[256];
        g.GetShaderInfoLog(s, sizeof log, nullptr, log);
        std::cerr << "kurvenrausch: gles shader: " << log << "\n";
        g.DeleteShader(s);
        return 0;
    }
    return s;
}

float exp_fog(float distance, float density) {
    return 1.f / std::exp(distance * distance * density);
}

int clip_row(float y) { return pixel_edge(y); }

Color fogged(Color c, Color air, float amount, float daylight) {
    Color f = fogged_color(c, air, amount);
    f.r = static_cast<uint8_t>(std::min(255.f, static_cast<float>(f.r) * daylight + 0.5f));
    f.g = static_cast<uint8_t>(std::min(255.f, static_cast<float>(f.g) * daylight + 0.5f));
    f.b = static_cast<uint8_t>(std::min(255.f, static_cast<float>(f.b) * daylight + 0.5f));
    return f;
}

} // namespace

GlesRenderer::~GlesRenderer() { shutdown(); }

void GlesRenderer::shutdown() {
    if (!g.DeleteTextures) {
        program_ = 0;
        vbo_ = 0;
        textures_.clear();
        return;
    }
    for (auto& [k, tex] : textures_) {
        (void)k;
        if (tex) g.DeleteTextures(1, &tex);
    }
    textures_.clear();
    if (vbo_) {
        g.DeleteBuffers(1, &vbo_);
        vbo_ = 0;
    }
    if (program_) {
        g.DeleteProgram(program_);
        program_ = 0;
    }
}

bool GlesRenderer::init() {
    if (program_) return true;
    if (!load_gl()) {
        std::cerr << "kurvenrausch: gles: missing GL entry points\n";
        return false;
    }
    const unsigned vs = compile(GL_VERTEX_SHADER_, k_vert);
    const unsigned fs = compile(GL_FRAGMENT_SHADER_, k_frag);
    if (!vs || !fs) return false;
    program_ = g.CreateProgram();
    g.AttachShader(program_, vs);
    g.AttachShader(program_, fs);
    g.BindAttribLocation(program_, 0, "a_pos");
    g.BindAttribLocation(program_, 1, "a_uv");
    g.BindAttribLocation(program_, 2, "a_col");
    g.LinkProgram(program_);
    g.DeleteShader(vs);
    g.DeleteShader(fs);
    int ok = 0;
    g.GetProgramiv(program_, GL_LINK_STATUS_, &ok);
    if (!ok) {
        char log[256];
        g.GetProgramInfoLog(program_, sizeof log, nullptr, log);
        std::cerr << "kurvenrausch: gles link: " << log << "\n";
        g.DeleteProgram(program_);
        program_ = 0;
        return false;
    }
    u_screen_ = g.GetUniformLocation(program_, "u_screen");
    u_use_tex_ = g.GetUniformLocation(program_, "u_use_tex");
    u_tex_ = g.GetUniformLocation(program_, "u_tex");
    g.GenBuffers(1, &vbo_);
    return true;
}

void GlesRenderer::set_size(int width, int height) {
    width_ = std::max(1, width);
    height_ = std::max(1, height);
}

void GlesRenderer::clear_batch() {
    solid_.clear();
    textured_.clear();
}

void GlesRenderer::push_trap(float y0, float x0l, float x0r, float y1, float x1l, float x1r, Color c) {
    const float r = c.r / 255.f, gch = c.g / 255.f, b = c.b / 255.f, a = c.a / 255.f;
    // two triangles: (x0l,y0)-(x0r,y0)-(x1l,y1) and (x0r,y0)-(x1r,y1)-(x1l,y1)
    const Vertex verts[6] = {
        {x0l, y0, 0, 0, r, gch, b, a}, {x0r, y0, 0, 0, r, gch, b, a}, {x1l, y1, 0, 0, r, gch, b, a},
        {x0r, y0, 0, 0, r, gch, b, a}, {x1r, y1, 0, 0, r, gch, b, a}, {x1l, y1, 0, 0, r, gch, b, a},
    };
    solid_.insert(solid_.end(), verts, verts + 6);
}

void GlesRenderer::push_quad(float x, float y, float w, float h, float u0, float v0, float u1, float v1, Color c,
                             bool flip) {
    if (flip) std::swap(u0, u1);
    const float r = c.r / 255.f, gch = c.g / 255.f, b = c.b / 255.f, a = c.a / 255.f;
    const float x1 = x + w, y1 = y + h;
    const Vertex verts[6] = {
        {x, y, u0, v0, r, gch, b, a}, {x1, y, u1, v0, r, gch, b, a}, {x, y1, u0, v1, r, gch, b, a},
        {x1, y, u1, v0, r, gch, b, a}, {x1, y1, u1, v1, r, gch, b, a}, {x, y1, u0, v1, r, gch, b, a},
    };
    textured_.insert(textured_.end(), verts, verts + 6);
}

void GlesRenderer::flush_solid() {
    if (solid_.empty()) return;
    g.Uniform1i(u_use_tex_, 0);
    g.BindBuffer(GL_ARRAY_BUFFER_, vbo_);
    g.BufferData(GL_ARRAY_BUFFER_, static_cast<std::ptrdiff_t>(solid_.size() * sizeof(Vertex)), solid_.data(),
                 GL_DYNAMIC_DRAW_);
    g.EnableVertexAttribArray(0);
    g.EnableVertexAttribArray(1);
    g.EnableVertexAttribArray(2);
    const int stride = static_cast<int>(sizeof(Vertex));
    g.VertexAttribPointer(0, 2, GL_FLOAT_, GL_FALSE_, stride, reinterpret_cast<void*>(0));
    g.VertexAttribPointer(1, 2, GL_FLOAT_, GL_FALSE_, stride, reinterpret_cast<void*>(sizeof(float) * 2));
    g.VertexAttribPointer(2, 4, GL_FLOAT_, GL_FALSE_, stride, reinterpret_cast<void*>(sizeof(float) * 4));
    g.DrawArrays(GL_TRIANGLES_, 0, static_cast<int>(solid_.size()));
    solid_.clear();
}

void GlesRenderer::flush_textured(unsigned tex) {
    if (textured_.empty()) return;
    g.Uniform1i(u_use_tex_, 1);
    g.ActiveTexture(GL_TEXTURE0_);
    g.BindTexture(GL_TEXTURE_2D_, tex);
    g.Uniform1i(u_tex_, 0);
    g.BindBuffer(GL_ARRAY_BUFFER_, vbo_);
    g.BufferData(GL_ARRAY_BUFFER_, static_cast<std::ptrdiff_t>(textured_.size() * sizeof(Vertex)), textured_.data(),
                 GL_DYNAMIC_DRAW_);
    g.EnableVertexAttribArray(0);
    g.EnableVertexAttribArray(1);
    g.EnableVertexAttribArray(2);
    const int stride = static_cast<int>(sizeof(Vertex));
    g.VertexAttribPointer(0, 2, GL_FLOAT_, GL_FALSE_, stride, reinterpret_cast<void*>(0));
    g.VertexAttribPointer(1, 2, GL_FLOAT_, GL_FALSE_, stride, reinterpret_cast<void*>(sizeof(float) * 2));
    g.VertexAttribPointer(2, 4, GL_FLOAT_, GL_FALSE_, stride, reinterpret_cast<void*>(sizeof(float) * 4));
    g.DrawArrays(GL_TRIANGLES_, 0, static_cast<int>(textured_.size()));
    textured_.clear();
}

unsigned GlesRenderer::texture_for(const Bitmap& bmp) {
    if (bmp.w <= 0 || bmp.h <= 0 || bmp.px.empty()) return 0;
    const uint32_t* key = bmp.px.data();
    if (auto it = textures_.find(key); it != textures_.end()) return it->second;
    unsigned tex = 0;
    g.GenTextures(1, &tex);
    g.BindTexture(GL_TEXTURE_2D_, tex);
    g.TexParameteri(GL_TEXTURE_2D_, GL_TEXTURE_MIN_FILTER_, GL_NEAREST_);
    g.TexParameteri(GL_TEXTURE_2D_, GL_TEXTURE_MAG_FILTER_, GL_NEAREST_);
    g.TexParameteri(GL_TEXTURE_2D_, GL_TEXTURE_WRAP_S_, GL_CLAMP_TO_EDGE_);
    g.TexParameteri(GL_TEXTURE_2D_, GL_TEXTURE_WRAP_T_, GL_CLAMP_TO_EDGE_);
    std::vector<uint8_t> rgba(static_cast<size_t>(bmp.w) * static_cast<size_t>(bmp.h) * 4);
    for (size_t i = 0; i < bmp.px.size(); ++i) {
        const uint32_t p = bmp.px[i];
        rgba[i * 4 + 0] = static_cast<uint8_t>((p >> 16) & 0xff);
        rgba[i * 4 + 1] = static_cast<uint8_t>((p >> 8) & 0xff);
        rgba[i * 4 + 2] = static_cast<uint8_t>(p & 0xff);
        rgba[i * 4 + 3] = static_cast<uint8_t>((p >> 24) & 0xff);
    }
    g.PixelStorei(0x0CF5 /* GL_UNPACK_ALIGNMENT */, 1);
    g.TexImage2D(GL_TEXTURE_2D_, 0, static_cast<int>(GL_RGBA_), bmp.w, bmp.h, 0, GL_RGBA_, GL_UNSIGNED_BYTE_,
                 rgba.data());
    textures_[key] = tex;
    return tex;
}

void GlesRenderer::project_point(ScreenPoint& p, float world_x, float world_y, float world_z, float cam_x, float cam_y,
                                 float cam_z, float depth, int direction, int screen_w, float x_scale, float horizon,
                                 float y_scale, float road_width) const {
    p.cam_z = (world_z - cam_z) * static_cast<float>(direction);
    p.scale = depth / p.cam_z;
    p.x = static_cast<float>(screen_w) / 2.f + p.scale * (world_x - cam_x) * x_scale;
    p.y = horizon - p.scale * (world_y - cam_y) * y_scale;
    p.w = p.scale * road_width * x_scale;
}

void GlesRenderer::draw_segment(const Track& track, const Slice& s, const RoadTheme& theme) {
    const ScreenPoint& a = direction_ > 0 ? s.p1 : s.p2;
    const ScreenPoint& b = direction_ > 0 ? s.p2 : s.p1;
    const Segment& seg = track.segment(s.index);
    const float fog_amount = 1.f - s.fog;
    const int near = direction_ > 0 ? s.index : s.index + 1;
    const int band = near % 2;
    const float wf = static_cast<float>(width_);
    auto fogc = [&](Color c) { return fogged(c, fog_air_, fog_amount, daylight_); };

    push_trap(b.y, 0.f, wf, a.y, 0.f, wf, fogc(seg.tunnel ? Color{0x6c, 0x68, 0x62} : theme.grass[band]));
    if (seg.tunnel) {
        const float ca = a.y - a.scale * tunnel_height * y_scale_, cb = b.y - b.scale * tunnel_height * y_scale_;
        push_trap(ca, 0.f, wf, cb, 0.f, wf, fogc(Color{0x34, 0x32, 0x30}));
    }
    const float ra = a.w / 6.f, rb = b.w / 6.f;
    const Color rumble = fogc(theme.rumble[band]);
    push_trap(b.y, b.x - b.w - rb, b.x - b.w, a.y, a.x - a.w - ra, a.x - a.w, rumble);
    push_trap(b.y, b.x + b.w, b.x + b.w + rb, a.y, a.x + a.w, a.x + a.w + ra, rumble);
    if (seg.checker) {
        for (int i = 0; i < 8; ++i) {
            const float f0 = static_cast<float>(i) / 8.f, f1 = static_cast<float>(i + 1) / 8.f;
            const Color c = fogc(theme.checker[(i + s.index) % 2]);
            push_trap(b.y, b.x - b.w + 2.f * b.w * f0, b.x - b.w + 2.f * b.w * f1, a.y, a.x - a.w + 2.f * a.w * f0,
                      a.x - a.w + 2.f * a.w * f1, c);
        }
    } else {
        push_trap(b.y, b.x - b.w, b.x + b.w, a.y, a.x - a.w, a.x + a.w, fogc(theme.road[band]));
        const Color lane = fogc(theme.lane);
        for (int i = 1; i < theme.lanes; ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(theme.lanes);
            const float xa = a.x - a.w + 2.f * a.w * t, xb = b.x - b.w + 2.f * b.w * t;
            const float la = a.w * 0.02f, lb = b.w * 0.02f;
            push_trap(b.y, xb - lb, xb + lb, a.y, xa - la, xa + la, lane);
        }
    }
}

void GlesRenderer::draw_sprites(const Track& track, const SpriteSheet& sprites, std::vector<RoadSprite>& objects) {
    const float seg_len = track.segment_length;
    for (auto it = slices_.rbegin(); it != slices_.rend(); ++it) {
        const Slice& s = *it;
        const bool projectable = s.p1.cam_z > camera_depth_;
        const Segment& seg = track.segment(s.index);
        const float fog_amount = 1.f - s.fog;
        const ScreenPoint& p0 = direction_ > 0 ? s.p1 : s.p2;

        // Cliff billboards (subsampled like software).
        auto draw_cliff = [&](int side, const Bitmap& cliff) {
            const Edge kind = side < 0 ? seg.left : seg.right;
            if (kind != Edge::Cliff || cliff.w <= 0) return;
            const float off = cliff_offset * static_cast<float>(side);
            const float xa = p0.x + off * p0.w;
            const int near = direction_ > 0 ? s.index : s.index + 1;
            const float h1 = track.edge_height(near, side);
            if (!(h1 > 50.f)) return;
            const float ppu = std::max(p0.scale * x_scale_, 1e-4f);
            const int stride = ppu > 0.12f ? 2 : 3;
            if ((s.index % stride) != 0) return;
            const float height = h1 * ppu;
            const float segs = static_cast<float>(stride) + 0.25f;
            const float width = std::max(track.segment_length * ppu * segs, height * 1.05f);
            if (!(height > 2.f) || !(width > 2.f)) return;
            const float left = side < 0 ? xa - width : xa;
            Color tint = fogged(Color{255, 255, 255}, fog_air_, fog_amount, daylight_);
            const unsigned tex = texture_for(cliff);
            if (!tex) return;
            flush_solid();
            push_quad(left, p0.y - height, width, height, 0.f, 0.f, 1.f, 1.f, tint, side < 0);
            flush_textured(tex);
        };
        if (projectable) {
            const bool snow = track.look(s.index).cap_amount > 0.45f;
            draw_cliff(-1, sprites.cliff_face(s.index, snow));
            draw_cliff(+1, sprites.cliff_face(s.index * 3 + 1, snow));
        }

        auto plant = [&](const RoadsideObject& obj, float shift) {
            if (!projectable) return;
            const SceneryInfo& info = scenery_info(obj.kind);
            const float px = p0.scale * x_scale_;
            const float width = info.width * px;
            float left = p0.x + (obj.offset + shift) * track.half_width(s.index) * px;
            if (info.centered) left -= width / 2.f;
            else if (obj.offset < 0.f) left -= width;
            const bool wake = window_wake_ > 0.f && p0.cam_z < window_wake_ && SpriteSheet::scenery_has_windows(obj.kind);
            const Bitmap& bmp = direction_ > 0 ? sprites.scenery(obj.kind, wake) : sprites.scenery_back(obj.kind, wake);
            float height = width * static_cast<float>(bmp.h) / static_cast<float>(std::max(1, bmp.w));
            if (obj.kind == Scenery::TunnelPortal) {
                constexpr float open_frac = 36.f / 128.f;
                height = (tunnel_height * p0.scale * y_scale_) / open_frac;
            }
            const bool flip = info.mirrorable && obj.offset < 0.f;
            Color tint = fogged(Color{255, 255, 255}, fog_air_, fog_amount, daylight_);
            const unsigned tex = texture_for(bmp);
            if (!tex) return;
            flush_solid();
            push_quad(left, p0.y - height, width, height, 0.f, 0.f, 1.f, 1.f, tint, flip);
            flush_textured(tex);
        };
        if (projectable) {
            for (const RoadsideObject& obj : seg.scenery) plant(obj, 0.f);
        }

        const float z0 = static_cast<float>(s.index) * seg_len;
        auto first = std::lower_bound(objects.begin(), objects.end(), z0,
                                      [](const RoadSprite& o, float z) { return o.z < z; });
        auto last = std::lower_bound(first, objects.end(), z0 + seg_len,
                                     [](const RoadSprite& o, float z) { return o.z < z; });
        auto draw_object = [&](const RoadSprite& o) {
            const Bitmap& bmp = *o.bitmap;
            if (o.fixed) {
                Color tint{255, 255, 255};
                tint.r = static_cast<uint8_t>(std::min(255.f, tint.r * daylight_ + 0.5f));
                tint.g = static_cast<uint8_t>(std::min(255.f, tint.g * daylight_ + 0.5f));
                tint.b = static_cast<uint8_t>(std::min(255.f, tint.b * daylight_ + 0.5f));
                const unsigned tex = texture_for(bmp);
                if (!tex) return;
                flush_solid();
                push_quad(o.sx, o.sy, o.sw, o.sh, 0.f, 0.f, 1.f, 1.f, tint, false);
                flush_textured(tex);
                return;
            }
            if (!projectable) return;
            const ScreenPoint& p1 = direction_ > 0 ? s.p2 : s.p1;
            const float t = (o.z - z0) / seg_len;
            const float scale = p0.scale + (p1.scale - p0.scale) * t;
            const float x = p0.x + (p1.x - p0.x) * t;
            const float y = p0.y + (p1.y - p0.y) * t;
            const float px = scale * x_scale_;
            const float width = o.world_width * px;
            const float height = width * static_cast<float>(bmp.h) / static_cast<float>(std::max(1, bmp.w));
            const float cx = x + o.offset * track.half_width_at(o.z) * px;
            Color tint = fogged(Color{255, 255, 255}, fog_air_, fog_amount, daylight_);
            const unsigned tex = texture_for(bmp);
            if (!tex) return;
            flush_solid();
            push_quad(cx - width / 2.f, y - height, width, height, 0.f, 0.f, 1.f, 1.f, tint, o.flip);
            flush_textured(tex);
        };
        if (direction_ > 0) {
            for (auto o = std::make_reverse_iterator(last); o != std::make_reverse_iterator(first); ++o)
                draw_object(*o);
        } else {
            for (auto o = first; o != last; ++o) draw_object(*o);
        }
    }
}

void GlesRenderer::render(const Track& track, const RoadView& view, const SpriteSheet& sprites,
                          std::vector<RoadSprite>& objects, const RoadTheme& theme, float daylight,
                          int viewport_w, int viewport_h) {
    if (!program_) return;
    daylight_ = std::clamp(daylight, 0.05f, 1.f);
    fog_air_ = view.fog_air;
    window_wake_ = view.window_wake;
    camera_depth_ = view.camera_depth;
    direction_ = view.direction >= 0 ? 1 : -1;
    const int dir = direction_;
    x_scale_ = view.x_scale > 0.f ? view.x_scale : static_cast<float>(width_) / 2.f;
    const float horizon = view.horizon > 0.f ? view.horizon : static_cast<float>(height_) / 2.f;
    y_scale_ = view.y_scale > 0.f ? view.y_scale : horizon;

    const int n_segments = static_cast<int>(track.segments.size());
    const float seg_len = track.segment_length;
    const float base_z = view.position;
    const int base = track.index_at(base_z);
    const float base_percent = (base_z - static_cast<float>(base) * seg_len) / seg_len;
    const float cam_y = view.player_y + view.camera_height;
    float x = 0.f;
    float dx = -track.segment(base).curve * (dir > 0 ? base_percent : 1.f - base_percent) + view.yaw;
    float max_y = static_cast<float>(height_);
    float ceiling = 0.f;
    float mouth_left = 0.f, mouth_right = static_cast<float>(width_);
    const int count = std::min(view.draw_distance, n_segments);
    slices_.clear();
    slices_.reserve(static_cast<size_t>(count));
    row_depth_.assign(static_cast<size_t>(height_), 0.f);
    lamps_.clear();

    for (int n = 0; n < count; ++n) {
        const int index = ((base + dir * n) % n_segments + n_segments) % n_segments;
        const Segment& seg = track.segment(index);
        float loop = 0.f;
        const int track_len = n_segments;
        if (dir > 0 && index < base) loop = static_cast<float>(track_len) * seg_len;
        if (dir < 0 && index > base) loop = -static_cast<float>(track_len) * seg_len;
        const float cam_z = view.position - loop;
        const float cam_x = view.player_x * track.half_width_at(view.position + view.player_z) + view.shift;
        const float z1 = static_cast<float>(index) * seg_len;
        const float z2 = z1 + seg_len;
        Slice s;
        s.index = index;
        const int near = dir > 0 ? index : index + 1;
        project_point(s.p1, x, dir > 0 ? seg.y1 : seg.y2, dir > 0 ? z1 : z2, cam_x, cam_y, cam_z, view.camera_depth, dir,
                      width_, x_scale_, horizon, y_scale_, track.half_width(near));
        project_point(s.p2, x + dx, dir > 0 ? seg.y2 : seg.y1, dir > 0 ? z2 : z1, cam_x, cam_y, cam_z, view.camera_depth,
                      dir, width_, x_scale_, horizon, y_scale_, track.half_width(near + dir));
        x += dx;
        dx += seg.curve;
        s.clip = max_y;
        s.top = ceiling;
        s.left = mouth_left;
        s.right = mouth_right;
        s.fog = exp_fog(static_cast<float>(n) / static_cast<float>(count), view.fog_density);
        s.road_visible = s.p1.cam_z > view.camera_depth && s.p2.y < s.p1.y && s.p2.y < max_y;
        if (s.road_visible) {
            const int y0 = std::max(0, pixel_edge(s.p2.y));
            const int y1 = std::min(height_, pixel_edge(std::min(s.p1.y, max_y)));
            for (int y = y0; y < y1; ++y) {
                const float t = (static_cast<float>(y) + 0.5f - s.p1.y) / (s.p2.y - s.p1.y);
                const float inv = 1.f / s.p1.cam_z + t * (1.f / s.p2.cam_z - 1.f / s.p1.cam_z);
                row_depth_[static_cast<size_t>(y)] = inv > 0.f ? 1.f / inv : 0.f;
            }
            max_y = s.p2.y;
        }
        slices_.push_back(s);
    }

    for (RoadSprite& o : objects) o.z = track.wrap(o.z);
    std::sort(objects.begin(), objects.end(), [](const RoadSprite& a, const RoadSprite& b) { return a.z < b.z; });

    // GL frame
    Color air = fog_air_;
    if (theme.haze >= 0.99f || theme.fog_density >= 80.f) {
        air = theme.fog;
    }
    g.Viewport(0, 0, width_, height_); // caller may scale via present; scene uses internal size
    g.ClearColor(air.r / 255.f * daylight_, air.g / 255.f * daylight_, air.b / 255.f * daylight_, 1.f);
    g.Clear(GL_COLOR_BUFFER_BIT_);
    g.Enable(GL_BLEND_);
    g.BlendFunc(GL_SRC_ALPHA_, GL_ONE_MINUS_SRC_ALPHA_);
    g.UseProgram(program_);
    g.Uniform2f(u_screen_, static_cast<float>(width_), static_cast<float>(height_));

    clear_batch();
    for (const Slice& s : slices_) {
        if (s.road_visible) draw_segment(track, s, theme);
    }
    flush_solid();
    draw_sprites(track, sprites, objects);
    flush_solid();
}

} // namespace racer
