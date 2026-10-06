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
constexpr unsigned GL_ONE_ = 1;
constexpr unsigned GL_ZERO_ = 0;
constexpr unsigned GL_DST_COLOR_ = 0x0306;
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
constexpr unsigned GL_FRAMEBUFFER_ = 0x8D40;
constexpr unsigned GL_COLOR_ATTACHMENT0_ = 0x8CE0;
constexpr unsigned GL_DEPTH_ATTACHMENT_ = 0x8D00;
constexpr unsigned GL_RENDERBUFFER_ = 0x8D41;
constexpr unsigned GL_DEPTH_COMPONENT16_ = 0x81A5;
constexpr unsigned GL_FRAMEBUFFER_COMPLETE_ = 0x8CD5;
constexpr unsigned GL_FRAMEBUFFER_BINDING_ = 0x8CA6;

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
    void (*Uniform3f)(int, float, float, float) = nullptr;
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
    void (*TexSubImage2D)(unsigned, int, int, int, int, int, unsigned, unsigned, const void*) = nullptr;
    void (*TexParameteri)(unsigned, unsigned, int) = nullptr;
    void (*PixelStorei)(unsigned, int) = nullptr;
    void (*GenFramebuffers)(int, unsigned*) = nullptr;
    void (*DeleteFramebuffers)(int, const unsigned*) = nullptr;
    void (*BindFramebuffer)(unsigned, unsigned) = nullptr;
    void (*FramebufferTexture2D)(unsigned, unsigned, unsigned, unsigned, int) = nullptr;
    void (*GenRenderbuffers)(int, unsigned*) = nullptr;
    void (*DeleteRenderbuffers)(int, const unsigned*) = nullptr;
    void (*BindRenderbuffer)(unsigned, unsigned) = nullptr;
    void (*RenderbufferStorage)(unsigned, unsigned, int, int) = nullptr;
    void (*FramebufferRenderbuffer)(unsigned, unsigned, unsigned, unsigned) = nullptr;
    unsigned (*CheckFramebufferStatus)(unsigned) = nullptr;
    void (*GetIntegerv)(unsigned, int*) = nullptr;
    void (*ReadPixels)(int, int, int, int, unsigned, unsigned, void*) = nullptr;
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
    g.Uniform3f = load<decltype(g.Uniform3f)>("glUniform3f");
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
    g.TexSubImage2D = load<decltype(g.TexSubImage2D)>("glTexSubImage2D");
    g.TexParameteri = load<decltype(g.TexParameteri)>("glTexParameteri");
    g.PixelStorei = load<decltype(g.PixelStorei)>("glPixelStorei");
    g.GenFramebuffers = load<decltype(g.GenFramebuffers)>("glGenFramebuffers");
    if (!g.GenFramebuffers) g.GenFramebuffers = load<decltype(g.GenFramebuffers)>("glGenFramebuffersOES");
    g.DeleteFramebuffers = load<decltype(g.DeleteFramebuffers)>("glDeleteFramebuffers");
    if (!g.DeleteFramebuffers) g.DeleteFramebuffers = load<decltype(g.DeleteFramebuffers)>("glDeleteFramebuffersOES");
    g.BindFramebuffer = load<decltype(g.BindFramebuffer)>("glBindFramebuffer");
    if (!g.BindFramebuffer) g.BindFramebuffer = load<decltype(g.BindFramebuffer)>("glBindFramebufferOES");
    g.FramebufferTexture2D = load<decltype(g.FramebufferTexture2D)>("glFramebufferTexture2D");
    if (!g.FramebufferTexture2D) g.FramebufferTexture2D = load<decltype(g.FramebufferTexture2D)>("glFramebufferTexture2DOES");
    g.GenRenderbuffers = load<decltype(g.GenRenderbuffers)>("glGenRenderbuffers");
    if (!g.GenRenderbuffers) g.GenRenderbuffers = load<decltype(g.GenRenderbuffers)>("glGenRenderbuffersOES");
    g.DeleteRenderbuffers = load<decltype(g.DeleteRenderbuffers)>("glDeleteRenderbuffers");
    if (!g.DeleteRenderbuffers) g.DeleteRenderbuffers = load<decltype(g.DeleteRenderbuffers)>("glDeleteRenderbuffersOES");
    g.BindRenderbuffer = load<decltype(g.BindRenderbuffer)>("glBindRenderbuffer");
    if (!g.BindRenderbuffer) g.BindRenderbuffer = load<decltype(g.BindRenderbuffer)>("glBindRenderbufferOES");
    g.RenderbufferStorage = load<decltype(g.RenderbufferStorage)>("glRenderbufferStorage");
    if (!g.RenderbufferStorage) g.RenderbufferStorage = load<decltype(g.RenderbufferStorage)>("glRenderbufferStorageOES");
    g.FramebufferRenderbuffer = load<decltype(g.FramebufferRenderbuffer)>("glFramebufferRenderbuffer");
    if (!g.FramebufferRenderbuffer) g.FramebufferRenderbuffer = load<decltype(g.FramebufferRenderbuffer)>("glFramebufferRenderbufferOES");
    g.CheckFramebufferStatus = load<decltype(g.CheckFramebufferStatus)>("glCheckFramebufferStatus");
    if (!g.CheckFramebufferStatus) g.CheckFramebufferStatus = load<decltype(g.CheckFramebufferStatus)>("glCheckFramebufferStatusOES");
    g.GetIntegerv = load<decltype(g.GetIntegerv)>("glGetIntegerv");
    g.ReadPixels = load<decltype(g.ReadPixels)>("glReadPixels");
    return g.Clear && g.CreateShader && g.DrawArrays && g.TexImage2D && g.GenFramebuffers && g.BindFramebuffer;
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
    "uniform vec3 u_fog_air;\n"
    "void main(){\n"
    "  if (u_use_tex != 0) {\n"
    "    vec4 t = texture2D(u_tex, v_uv);\n"
    "    if (t.a < 0.01) discard;\n"
    "    vec3 lit = t.rgb * v_col.rgb;\n"
    "    float fa = v_col.a;\n"
    "    float lum = max(t.r, max(t.g, t.b));\n"
    "    if (lum > 0.72 && t.a > 0.9) fa = 0.0;\n"
    "    gl_FragColor = vec4(mix(lit, u_fog_air, fa), t.a);\n"
    "  } else {\n"
    "    gl_FragColor = v_col;\n"
    "  }\n"
    "}\n";
#else
    "#version 110\n"
    "varying vec2 v_uv;\n"
    "varying vec4 v_col;\n"
    "uniform sampler2D u_tex;\n"
    "uniform int u_use_tex;\n"
    "uniform vec3 u_fog_air;\n"
    "void main(){\n"
    "  if (u_use_tex != 0) {\n"
    "    vec4 t = texture2D(u_tex, v_uv);\n"
    "    if (t.a < 0.01) discard;\n"
    "    vec3 lit = t.rgb * v_col.rgb;\n"
    "    float fa = v_col.a;\n"
    "    float lum = max(t.r, max(t.g, t.b));\n"
    "    if (lum > 0.72 && t.a > 0.9) fa = 0.0;\n"
    "    gl_FragColor = vec4(mix(lit, u_fog_air, fa), t.a);\n"
    "  } else {\n"
    "    gl_FragColor = v_col;\n"
    "  }\n"
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

void GlesRenderer::invalidate() {
    // Context is gone: do not call glDelete*. Zero handles so init() rebuilds.
    program_ = 0;
    vbo_ = 0;
    fbo_ = 0;
    color_tex_ = 0;
    depth_rb_ = 0;
    light_fbo_ = 0;
    light_tex_ = 0;
    fbo_w_ = fbo_h_ = 0;
    textures_.clear();
    // Force load_gl to re-resolve entry points if needed.
    g = {};
}

void GlesRenderer::shutdown() {
    if (!g.DeleteTextures) {
        program_ = 0;
        vbo_ = 0;
        fbo_ = color_tex_ = depth_rb_ = light_fbo_ = light_tex_ = 0;
        textures_.clear();
        return;
    }
    for (auto& [k, entry] : textures_) {
        (void)k;
        if (entry.id) g.DeleteTextures(1, &entry.id);
    }
    textures_.clear();
    if (depth_rb_ && g.DeleteRenderbuffers) {
        g.DeleteRenderbuffers(1, &depth_rb_);
        depth_rb_ = 0;
    }
    if (color_tex_) {
        g.DeleteTextures(1, &color_tex_);
        color_tex_ = 0;
    }
    if (fbo_ && g.DeleteFramebuffers) {
        g.DeleteFramebuffers(1, &fbo_);
        fbo_ = 0;
    }
    fbo_w_ = fbo_h_ = 0;
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
    u_fog_air_ = g.GetUniformLocation(program_, "u_fog_air");
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

void GlesRenderer::push_trap_vcol(float y0, float x0l, float x0r, float y1, float x1l, float x1r, Color c0, Color c1) {
    const float r0 = c0.r / 255.f, g0 = c0.g / 255.f, b0 = c0.b / 255.f, a0 = c0.a / 255.f;
    const float r1 = c1.r / 255.f, g1 = c1.g / 255.f, b1 = c1.b / 255.f, a1 = c1.a / 255.f;
    const Vertex verts[6] = {
        {x0l, y0, 0, 0, r0, g0, b0, a0}, {x0r, y0, 0, 0, r0, g0, b0, a0}, {x1l, y1, 0, 0, r1, g1, b1, a1},
        {x0r, y0, 0, 0, r0, g0, b0, a0}, {x1r, y1, 0, 0, r1, g1, b1, a1}, {x1l, y1, 0, 0, r1, g1, b1, a1},
    };
    solid_.insert(solid_.end(), verts, verts + 6);
}

// General solid quad (two triangles). Vertices in order around the perimeter.
void GlesRenderer::push_solid_quad(float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3,
                                   Color c) {
    const float r = c.r / 255.f, gch = c.g / 255.f, b = c.b / 255.f, a = c.a / 255.f;
    const Vertex verts[6] = {
        {x0, y0, 0, 0, r, gch, b, a}, {x1, y1, 0, 0, r, gch, b, a}, {x2, y2, 0, 0, r, gch, b, a},
        {x0, y0, 0, 0, r, gch, b, a}, {x2, y2, 0, 0, r, gch, b, a}, {x3, y3, 0, 0, r, gch, b, a},
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

// Axis-aligned rect rotated about its centre (matches Framebuffer::blit_rotated).
void GlesRenderer::push_quad_rotated(float cx, float cy, float w, float h, float angle, float u0, float v0, float u1,
                                     float v1, Color c) {
    const float r = c.r / 255.f, gch = c.g / 255.f, b = c.b / 255.f, a = c.a / 255.f;
    const float cos_a = std::cos(angle), sin_a = std::sin(angle);
    const float hw = w * 0.5f, hh = h * 0.5f;
    auto corner = [&](float lx, float ly, float u, float v) -> Vertex {
        return {cx + lx * cos_a - ly * sin_a, cy + lx * sin_a + ly * cos_a, u, v, r, gch, b, a};
    };
    // Local corners: TL TR BL BR (y down, same as screen).
    const Vertex tl = corner(-hw, -hh, u0, v0);
    const Vertex tr = corner(hw, -hh, u1, v0);
    const Vertex bl = corner(-hw, hh, u0, v1);
    const Vertex br = corner(hw, hh, u1, v1);
    const Vertex verts[6] = {tl, tr, bl, tr, br, bl};
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

unsigned GlesRenderer::texture_for(const Bitmap& bmp, bool dynamic) {
    if (bmp.w <= 0 || bmp.h <= 0 || bmp.px.empty() || !g.GenTextures) return 0;
    const uint32_t* key = bmp.px.data();

    auto upload_rgba = [&](unsigned tex, bool allocate) {
        g.BindTexture(GL_TEXTURE_2D_, tex);
        if (allocate) {
            g.TexParameteri(GL_TEXTURE_2D_, GL_TEXTURE_MIN_FILTER_, GL_NEAREST_);
            g.TexParameteri(GL_TEXTURE_2D_, GL_TEXTURE_MAG_FILTER_, GL_NEAREST_);
            g.TexParameteri(GL_TEXTURE_2D_, GL_TEXTURE_WRAP_S_, GL_CLAMP_TO_EDGE_);
            g.TexParameteri(GL_TEXTURE_2D_, GL_TEXTURE_WRAP_T_, GL_CLAMP_TO_EDGE_);
        }
        std::vector<uint8_t> rgba(static_cast<size_t>(bmp.w) * static_cast<size_t>(bmp.h) * 4);
        for (size_t i = 0; i < bmp.px.size(); ++i) {
            const uint32_t p = bmp.px[i];
            rgba[i * 4 + 0] = static_cast<uint8_t>((p >> 16) & 0xff);
            rgba[i * 4 + 1] = static_cast<uint8_t>((p >> 8) & 0xff);
            rgba[i * 4 + 2] = static_cast<uint8_t>(p & 0xff);
            rgba[i * 4 + 3] = static_cast<uint8_t>((p >> 24) & 0xff);
        }
        g.PixelStorei(0x0CF5 /* GL_UNPACK_ALIGNMENT */, 1);
        if (allocate || !g.TexSubImage2D) {
            g.TexImage2D(GL_TEXTURE_2D_, 0, static_cast<int>(GL_RGBA_), bmp.w, bmp.h, 0, GL_RGBA_, GL_UNSIGNED_BYTE_,
                         rgba.data());
        } else {
            g.TexSubImage2D(GL_TEXTURE_2D_, 0, 0, 0, bmp.w, bmp.h, GL_RGBA_, GL_UNSIGNED_BYTE_, rgba.data());
        }
    };

    if (auto it = textures_.find(key); it != textures_.end()) {
        CachedTex& entry = it->second;
        if (entry.id && entry.w == bmp.w && entry.h == bmp.h) {
            // Immutable: keep GPU copy. Dynamic (player composite): refresh pixels.
            if (dynamic) upload_rgba(entry.id, false);
            return entry.id;
        }
        if (entry.id) g.DeleteTextures(1, &entry.id);
        entry = {};
    }
    unsigned tex = 0;
    g.GenTextures(1, &tex);
    upload_rgba(tex, true);
    textures_[key] = CachedTex{tex, bmp.w, bmp.h};
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


void GlesRenderer::draw_backdrop(const RoadTheme& theme, const Background* backdrop, float hour, float horizon) {
    // Extreme fog: solid air only (already cleared).
    if (theme.haze >= 0.99f || theme.fog_density >= 80.f) return;
    const float wf = static_cast<float>(width_);
    // Sky gradient: pure sky_top at the zenith; only the horizon end picks up
    // haze (blend toward atmosphere_air). Matches the idea that sky_horizon is
    // already the near-horizon colour, with theme.haze adding extra wash.
    {
        const Color air = atmosphere_air(theme);
        const Color top = theme.sky_top; // no haze at the top
        const Color bot = blend(theme.sky_horizon, air, std::clamp(theme.haze, 0.f, 1.f));
        push_trap_vcol(0.f, 0.f, wf, horizon, 0.f, wf, top, bot);
    }
    if (!backdrop) {
        flush_solid();
        return;
    }
    // Mountain / hill columns. Snow only on the upper altitude band (software:
    // alt = horizon - y > snow_line), not the whole column.
    auto ridge2 = [&](const std::vector<float>& h, float offset, Color lit, Color shade, float scale_mul,
                      float snow_line) {
        if (h.empty()) return;
        const int period = static_cast<int>(h.size());
        const int step = std::max(1, width_ / 200);
        for (int x = 0; x < width_; x += step) {
            const int i = ((static_cast<int>(std::lround(offset)) + x) % period + period) % period;
            const float here = h[static_cast<size_t>(i)] * scale_mul;
            const float top_y = horizon - here;
            if (top_y >= horizon - 0.5f) continue;
            const float slope = h[static_cast<size_t>((i + 6) % period)] - h[static_cast<size_t>((i + period - 6) % period)];
            const float light = std::clamp(0.5f - slope * 0.08f, 0.f, 1.f);
            const Color rock = blend(shade, lit, light);
            const Color snow = light > 0.45f ? theme.snow : blend(theme.snow, theme.mountain_shade, 0.5f);
            const float x0 = static_cast<float>(x);
            const float x1 = std::min(wf, static_cast<float>(x + step));
            // Screen row where altitude == snow_line (snow above, rock below).
            const float snow_y = horizon - snow_line;
            auto haze_at = [&](float alt) {
                return std::max(theme.haze, std::clamp(1.f - alt / (14.f * scale_mul + 1.f), 0.f, 1.f) * 0.75f);
            };
            if (top_y < snow_y && snow_line < 1.0e8f) {
                // Cap: top → snow_y
                const float mid_alt = (horizon - top_y + snow_line) * 0.5f;
                const Color cs = fogged(snow, fog_air_, haze_at(mid_alt), daylight_);
                push_trap(top_y, x0, x1, snow_y, x0, x1, cs);
                // Rock: snow_y → horizon (smooth shade via vertical gradient optional)
                const float rock_alt = snow_line * 0.5f;
                const Color cr = fogged(rock, fog_air_, haze_at(rock_alt), daylight_);
                push_trap(snow_y, x0, x1, horizon, x0, x1, cr);
            } else {
                const Color c = fogged(rock, fog_air_, haze_at(here * 0.5f), daylight_);
                push_trap(top_y, x0, x1, horizon, x0, x1, c);
            }
        }
    };
    ridge2(backdrop->mountains(), backdrop->mountain_offset(), theme.mountain_lit, theme.mountain_shade,
           theme.mountain_scale, theme.snow_line);
    ridge2(backdrop->hills(), backdrop->hill_offset(), theme.hill_lit, theme.hill_shade, theme.hill_scale,
           1.0e9f); // hills never snow
    flush_solid();

    // Stars as solid 2×2 traps.
    if (theme.stars > 0.02f) {
        // Exact emissive colours (no fog) so apply_daylight keeps them lit.
        uint32_t seed = 0x51a7f00du;
        for (int i = 0; i < 90; ++i) {
            seed = seed * 1664525u + 1013904223u;
            const float x = static_cast<float>((seed >> 8) % static_cast<uint32_t>(width_));
            seed = seed * 1664525u + 1013904223u;
            const float y = static_cast<float>((seed >> 8) % 1000u) / 1000.f * horizon * 0.9f;
            if (static_cast<float>((seed >> 4) & 0xff) / 255.f > theme.stars) continue;
            const bool bright = (seed >> 28) < 4;
            const Color c = bright ? Color{0xe8, 0xee, 0xff} : Color{0xb8, 0xc8, 0xff};
            push_trap(y, x, x + 1.f, y + 1.f, x, x + 1.f, c);
        }
        flush_solid();
    }

    // Soft filled disc as concentric rings (smooth radial gradient).
    auto disc_soft = [&](float cx, float cy, float radius, Color core, Color edge, int rings) {
        for (int i = rings - 1; i >= 0; --i) {
            const float t0 = static_cast<float>(i) / static_cast<float>(rings);
            const float t1 = static_cast<float>(i + 1) / static_cast<float>(rings);
            const float r_out = radius * t1;
            const Color c = blend(core, edge, (t0 + t1) * 0.5f);
            const int y0 = std::max(0, static_cast<int>(cy - r_out));
            const int y1 = std::min(static_cast<int>(horizon), static_cast<int>(cy + r_out) + 1);
            for (int y = y0; y < y1; ++y) {
                const float dy = (static_cast<float>(y) + 0.5f - cy) / r_out;
                if (dy * dy >= 1.f) continue;
                const float half = r_out * std::sqrt(1.f - dy * dy);
                push_trap(static_cast<float>(y), cx - half, cx + half, static_cast<float>(y + 1), cx - half, cx + half, c);
            }
        }
    };

    const SkyBody sun = sun_position(hour);
    const SkyBody moon = moon_position(hour);
    const float half_w = wf * 0.5f;
    // Match Background::body_screen placement (0.88 band).
    auto body_xy = [&](const SkyBody& body, float& sx, float& sy) -> bool {
        if (body.elevation < -0.08f) return false;
        const float elev = std::clamp(body.elevation, 0.f, 1.f);
        const float band = horizon * 0.88f;
        sx = half_w + body.azimuth * half_w * 0.88f;
        sy = horizon - elev * band;
        return true;
    };

    float sun_sx = 0.f, sun_sy = 0.f;
    if (theme.sun_amount > 0.02f && body_xy(sun, sun_sx, sun_sy) && sun.elevation > -0.02f) {
        const float low = std::clamp(1.f - sun.elevation, 0.f, 1.f);
        const float radius = 5.f + 4.f * theme.sun_amount + 6.f * low;
        // Core brighter; edge theme.sun — software blends toward white at centre.
        const Color core = blend(theme.sun, Color{255, 255, 255}, 0.35f);
        const Color edge = fogged(theme.sun, fog_air_, theme.haze * 0.1f, daylight_);
        disc_soft(sun_sx, sun_sy, radius, core, edge, 6);
        // Soft glow halo (software dithers; we use low-alpha concentric rings).
        const float glow_r = radius * (1.8f + 0.6f * low);
        const Color glow = blend(theme.sky_horizon, theme.sun, 0.55f);
        Color g = glow;
        g.a = static_cast<uint8_t>(std::min(255.f, (0.25f + 0.2f * low) * theme.sun_amount * 180.f));
        const int y0 = std::max(0, static_cast<int>(sun_sy - glow_r));
        const int y1 = std::min(static_cast<int>(horizon), static_cast<int>(sun_sy + glow_r) + 1);
        for (int y = y0; y < y1; ++y) {
            const float dy = (static_cast<float>(y) + 0.5f - sun_sy) / glow_r;
            if (dy * dy >= 1.f) continue;
            const float half = glow_r * std::sqrt(1.f - dy * dy);
            // Only the ring outside the solid disc.
            const float dy2 = (static_cast<float>(y) + 0.5f - sun_sy) / radius;
            float inner = 0.f;
            if (dy2 * dy2 < 1.f) inner = radius * std::sqrt(1.f - dy2 * dy2);
            if (half <= inner + 0.5f) continue;
            push_trap(static_cast<float>(y), sun_sx - half, sun_sx - inner, static_cast<float>(y + 1), sun_sx - half,
                      sun_sx - inner, g);
            push_trap(static_cast<float>(y), sun_sx + inner, sun_sx + half, static_cast<float>(y + 1), sun_sx + inner,
                      sun_sx + half, g);
        }
        flush_solid();
    }

    float moon_sx = 0.f, moon_sy = 0.f;
    const bool moon_show = body_xy(moon, moon_sx, moon_sy) && moon.elevation > 0.02f &&
                           (theme.stars > 0.05f || sun.elevation < 0.25f);
    if (moon_show) {
        const float radius = 5.5f;
        const Color disc_c{0xe0, 0xe4, 0xec}, limb{0xb0, 0xb8, 0xc8};
        // Left-shaded disc: two half soft discs (limb on the left, bright on the right).
        disc_soft(moon_sx - radius * 0.15f, moon_sy, radius * 0.95f, limb, limb, 3);
        disc_soft(moon_sx + radius * 0.1f, moon_sy, radius, disc_c, blend(limb, disc_c, 0.5f), 5);
        if (theme.stars > 0.2f) {
            Color halo{0xc8, 0xd0, 0xe0, 40};
            const float hr = radius * 1.35f;
            const int y0 = std::max(0, static_cast<int>(moon_sy - hr));
            const int y1 = std::min(static_cast<int>(horizon), static_cast<int>(moon_sy + hr) + 1);
            for (int y = y0; y < y1; ++y) {
                const float dy = (static_cast<float>(y) + 0.5f - moon_sy) / hr;
                if (dy * dy >= 1.f) continue;
                const float half = hr * std::sqrt(1.f - dy * dy);
                const float dy2 = (static_cast<float>(y) + 0.5f - moon_sy) / radius;
                float inner = 0.f;
                if (dy2 * dy2 < 1.f) inner = radius * std::sqrt(1.f - dy2 * dy2);
                if (half <= inner + 0.5f) continue;
                push_trap(static_cast<float>(y), moon_sx - half, moon_sx - inner, static_cast<float>(y + 1),
                          moon_sx - half, moon_sx - inner, halo);
                push_trap(static_cast<float>(y), moon_sx + inner, moon_sx + half, static_cast<float>(y + 1),
                          moon_sx + inner, moon_sx + half, halo);
            }
        }
        flush_solid();
    }

    // Clouds
    const float period = Background::sky_layer_period;
    const float offset = backdrop->sky_offset();
    for (const Background::CloudSprite& c : backdrop->cloud_sprites()) {
        if (!c.bitmap || c.bitmap->w <= 0) continue;
        const unsigned tex = texture_for(*c.bitmap);
        if (!tex) continue;
        const float bw = static_cast<float>(c.bitmap->w);
        const float bh = static_cast<float>(c.bitmap->h);
        auto wrap = [](float v, float p) {
            v = std::fmod(v, p);
            return v < 0.f ? v + p : v;
        };
        const float x = wrap(c.x - offset, period);
        for (float rep : {x, x - period}) {
            if (rep + bw <= 0.f || rep >= wf) continue;
            // Tint only (software blit_scaled with cloud_tint_amount) — no distance fog.
            Color tint = blend(Color{255, 255, 255}, theme.cloud_tint, theme.cloud_tint_amount);
            tint.a = 0; // fog amount 0
            push_quad(rep, horizon - c.altitude, bw, bh, 0.f, 0.f, 1.f, 1.f, tint, false);
            flush_textured(tex);
        }
    }
}

void GlesRenderer::draw_segment(const Track& track, const Slice& s, const RoadTheme& theme) {
    const ScreenPoint& a = direction_ > 0 ? s.p1 : s.p2;
    const ScreenPoint& b = direction_ > 0 ? s.p2 : s.p1;
    const Segment& seg = track.segment(s.index);
    const float fog_amount = 1.f - s.fog;
    const int near = direction_ > 0 ? s.index : s.index + 1;
    const int band = seg.alt ? 0 : 1;
    const int lanes = std::max(1, theme.lanes);
    const float wf = static_cast<float>(width_);
    auto fogc = [&](Color c) { return fogged(c, fog_air_, fog_amount, daylight_); };

    const Color wall[2] = {{0x6c, 0x68, 0x62}, {0x60, 0x5c, 0x56}};
    push_trap(b.y, 0.f, wf, a.y, 0.f, wf, fogc(seg.tunnel ? wall[band] : theme.grass[band]));
    if (seg.tunnel) {
        const float ca = a.y - a.scale * tunnel_height * y_scale_, cb = b.y - b.scale * tunnel_height * y_scale_;
        push_trap(ca, 0.f, wf, cb, 0.f, wf, fogc(Color{0x34, 0x32, 0x30}));
        // Ceiling lamps every few segments (software uses unfogged warm yellow).
        if (s.index % 6 == 0) {
            const float lw = a.w * 0.08f;
            push_trap(ca, a.x - lw, a.x + lw, std::max(cb, ca + 1.f), b.x - lw, b.x + lw,
                      Color{0xff, 0xec, 0xb0});
        }
    }

    // Ground beyond a rail or cliff: sea/valley or rock (mostly hidden by the edge).
    for (int side = -1; side <= 1; side += 2) {
        const Edge kind = side < 0 ? seg.left : seg.right;
        if (kind == Edge::None) continue;
        const float off = kind == Edge::Rail ? rail_offset : cliff_offset;
        const Color c = kind == Edge::Rail ? fogc(theme.beyond[band]) : fogc(theme.rock[0]);
        const float xa = a.x + static_cast<float>(side) * off * a.w;
        const float xb = b.x + static_cast<float>(side) * off * b.w;
        if (side < 0) push_trap(b.y, 0.f, xb, a.y, 0.f, xa, c);
        else push_trap(b.y, xb, wf, a.y, xa, wf, c);
    }

    // Tunnel mouth: solid wall around the opening so scenery cannot show past
    // the aperture (way in from outside, way out from inside).
    {
        const int dir = direction_;
        const bool way_out = seg.tunnel && !track.segment(s.index + dir).tunnel;
        const bool way_in = seg.tunnel && !track.segment(s.index - dir).tunnel;
        if (way_out || way_in) {
            const ScreenPoint& mouth = way_out ? b : a;
            const float half = mouth.scale * tunnel_half_width * track.road_width * x_scale_;
            const float x0 = mouth.x - half, x1 = mouth.x + half;
            const float ceil_y = mouth.y - mouth.scale * tunnel_height * y_scale_;
            const Color mouth_wall = fogc(Color{0x6c, 0x68, 0x62});
            // Full-height sides and a header above the opening.
            push_trap(ceil_y, 0.f, x0, mouth.y, 0.f, x0, mouth_wall);
            push_trap(ceil_y, x1, wf, mouth.y, x1, wf, mouth_wall);
            push_trap(0.f, 0.f, wf, ceil_y, 0.f, wf, mouth_wall);
        }
    }

    const float oa = track.branch_offset(near), ob = track.branch_offset(near + direction_);
    const bool other_road = !std::isnan(oa) && !std::isnan(ob);
    const float oca = other_road ? a.x + oa * a.w : 0.f, ocb = other_road ? b.x + ob * b.w : 0.f;

    // Forecourt paving
    const float court_a = track.forecourt_at(near), court_b = track.forecourt_at(near + direction_);
    if (court_a > 1.f || court_b > 1.f) {
        const float fa = std::max(court_a, 1.f), fb_ = std::max(court_b, 1.f);
        const float side = static_cast<float>(seg.court_side);
        const Color paving = fogc(blend(theme.road[band], Color{0xb4, 0xb0, 0xa8}, 0.3f));
        const float a0 = a.x + side * 1.f * a.w, a1 = a.x + side * fa * a.w;
        const float b0 = b.x + side * 1.f * b.w, b1 = b.x + side * fb_ * b.w;
        push_trap(b.y, std::min(b0, b1), std::max(b0, b1), a.y, std::min(a0, a1), std::max(a0, a1), paving);
    }

    const float ra = a.w / static_cast<float>(std::max(6, 2 * lanes));
    const float rb = b.w / static_cast<float>(std::max(6, 2 * lanes));
    const Color rumble = fogc(theme.rumble[band]);
    if (other_road) {
        push_trap(b.y, ocb - b.w - rb, ocb + b.w + rb, a.y, oca - a.w - ra, oca + a.w + ra, rumble);
    }
    push_trap(b.y, b.x - b.w - rb, b.x - b.w, a.y, a.x - a.w - ra, a.x - a.w, rumble);
    push_trap(b.y, b.x + b.w, b.x + b.w + rb, a.y, a.x + a.w, a.x + a.w + ra, rumble);
    if (other_road) {
        push_trap(b.y, ocb - b.w, ocb + b.w, a.y, oca - a.w, oca + a.w, fogc(theme.road[band]));
    }

    if (seg.checker) {
        for (int i = 0; i < 8; ++i) {
            const float f0 = static_cast<float>(i) / 8.f, f1 = static_cast<float>(i + 1) / 8.f;
            const Color c = fogc(theme.checker[(i + s.index) % 2]);
            push_trap(b.y, b.x - b.w + 2.f * b.w * f0, b.x - b.w + 2.f * b.w * f1, a.y, a.x - a.w + 2.f * a.w * f0,
                      a.x - a.w + 2.f * a.w * f1, c);
        }
    } else {
        push_trap(b.y, b.x - b.w, b.x + b.w, a.y, a.x - a.w, a.x + a.w, fogc(theme.road[band]));
        if (theme.us_markings) {
            const Color white = fogc(theme.lane);
            const float e = 0.93f, th = 1.f / 60.f;
            for (int side = -1; side <= 1; side += 2) {
                const float sd = static_cast<float>(side);
                push_trap(b.y, b.x + sd * e * b.w - b.w * th, b.x + sd * e * b.w + b.w * th, a.y,
                          a.x + sd * e * a.w - a.w * th, a.x + sd * e * a.w + a.w * th, white);
            }
            if (lanes == 2) {
                const Color yellow = fogc(theme.center_line);
                const float gap = 0.035f, yt = 1.f / 80.f;
                for (int side = -1; side <= 1; side += 2) {
                    const float sd = static_cast<float>(side);
                    push_trap(b.y, b.x + sd * gap * b.w - b.w * yt, b.x + sd * gap * b.w + b.w * yt, a.y,
                              a.x + sd * gap * a.w - a.w * yt, a.x + sd * gap * a.w + a.w * yt, yellow);
                }
            }
        } else if (seg.alt && lanes > 1) {
            // Same half-width as software: road_w / max(32, 8*lanes).
            const Color lane = fogc(theme.lane);
            const float la = a.w / static_cast<float>(std::max(32, 8 * lanes));
            const float lb = b.w / static_cast<float>(std::max(32, 8 * lanes));
            for (int i = 1; i < lanes; ++i) {
                const float f = static_cast<float>(i) / static_cast<float>(lanes);
                const float xa = a.x - a.w + 2.f * a.w * f, xb = b.x - b.w + 2.f * b.w * f;
                push_trap(b.y, xb - lb, xb + lb, a.y, xa - la, xa + la, lane);
            }
        }
        if (other_road && seg.alt) {
            const float la = a.w / static_cast<float>(std::max(32, 8 * lanes));
            const float lb = b.w / static_cast<float>(std::max(32, 8 * lanes));
            for (int i = 1; i < lanes; ++i) {
                const float f = static_cast<float>(i) / static_cast<float>(lanes);
                const float xa = oca - a.w + 2.f * a.w * f, xb = ocb - b.w + 2.f * b.w * f;
                push_trap(b.y, xb - lb, xb + lb, a.y, xa - la, xa + la,
                          fogc(theme.us_markings ? theme.center_line : theme.lane));
            }
        }
    }

    // Oil / water patches on top of the road (software draws them after the
    // surface; drawing them earlier left them fully covered by the road trap).
    {
        const float wa = track.patch_width_at(near), wb = track.patch_width_at(near + direction_);
        if (seg.patch != Patch::None && (wa > 0.f || wb > 0.f)) {
            const float ca = track.patch_center_at(near), cb = track.patch_center_at(near + direction_);
            const float xa = a.x + ca * a.w, xb = b.x + cb * b.w;
            auto band_of = [&](float from, float to, Color c) {
                push_trap(b.y, xb + from * wb * b.w, xb + to * wb * b.w, a.y, xa + from * wa * a.w,
                          xa + to * wa * a.w, fogc(c));
            };
            const int glint = s.index % 4;
            if (seg.patch == Patch::Oil) {
                const Color slick{0x16, 0x14, 0x1a};
                const Color sheen[3] = {{0x6c, 0x3c, 0x7c}, {0x2c, 0x74, 0x7c}, {0x8c, 0x7c, 0x34}};
                band_of(-1.f, 1.f, slick);
                band_of(-0.55f, -0.35f, blend(slick, sheen[s.index % 3], 0.8f));
                band_of(0.05f, 0.2f, blend(slick, sheen[(s.index + 1) % 3], 0.7f));
                if (glint == 2) band_of(0.4f, 0.55f, blend(slick, sheen[(s.index + 2) % 3], 0.6f));
            } else {
                // Cooler, slightly brighter than the asphalt so wet patches
                // read as water without the software sky-mirror bands.
                const Color edge = blend(theme.road[band], Color{0x18, 0x28, 0x38}, 0.55f);
                const Color mid = blend(edge, Color{0x50, 0x78, 0x98}, 0.45f);
                const Color glint_c = blend(mid, Color{0xe8, 0xf0, 0xff}, 0.55f);
                band_of(-1.f, 1.f, edge);
                band_of(-0.7f, 0.7f, mid);
                if (glint == 1) band_of(-0.4f, -0.22f, glint_c);
                if (glint == 3) band_of(0.15f, 0.32f, glint_c);
            }
        }
    }

    // Railway crossing the road and the land on either side.
    if (seg.rails) {
        const auto across = [&](float t0, float t1, Color c) {
            const float y0 = a.y + (b.y - a.y) * t1, y1 = a.y + (b.y - a.y) * t0;
            if (y1 - y0 < 1.f) {
                push_trap(y0, 0.f, wf, y0 + 1.f, 0.f, wf, c);
                return;
            }
            push_trap(y0, 0.f, wf, y1, 0.f, wf, c);
        };
        across(0.15f, 0.85f, fogc(Color{0x3c, 0x30, 0x28}));
        across(0.28f, 0.36f, fogc(Color{0xb8, 0xbc, 0xc4}));
        across(0.64f, 0.72f, fogc(Color{0xb8, 0xbc, 0xc4}));
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

        // Clip to the tunnel mouth / nearer-road occlusion (software set_clip).
        // GL scissor origin is bottom-left; our y grows downward.
        const int clip_x0 = std::max(0, static_cast<int>(s.left));
        const int clip_x1 = std::min(width_, static_cast<int>(std::ceil(s.right)));
        const int clip_y0 = std::max(0, pixel_edge(s.top));
        const int clip_y1 = std::min(height_, clip_row(s.clip));
        const bool use_scissor = clip_x1 > clip_x0 && clip_y1 > clip_y0 &&
                                 (clip_x0 > 0 || clip_x1 < width_ || clip_y0 > 0 || clip_y1 < height_);
        if (use_scissor && g.Scissor && g.Enable) {
            flush_solid();
            g.Enable(GL_SCISSOR_TEST_);
            g.Scissor(clip_x0, height_ - clip_y1, clip_x1 - clip_x0, clip_y1 - clip_y0);
        }

        // Guard rails: same bands as software RoadRenderer::draw_edge (posts +
        // upper/lower bars with gaps). Drawn far→near so nearer rails win.
        auto draw_rail = [&](int side) {
            const Edge kind = side < 0 ? seg.left : seg.right;
            if (kind != Edge::Rail) return;
            // Match software: always p1 = near-side of slice walk, p2 = far.
            const ScreenPoint& a = s.p1;
            const ScreenPoint& b = s.p2;
            const float off = rail_offset * static_cast<float>(side);
            const float xa = a.x + off * a.w, xb = b.x + off * b.w;
            if (std::abs(xb - xa) < 0.01f) return; // edge-on
            const int near = direction_ > 0 ? s.index : s.index + 1;
            const float h1 = track.edge_height(near, side);
            const float h2 = track.edge_height(near + direction_, side);
            if (!(h1 > 1.f) && !(h2 > 1.f)) return;
            const float ppu_a = a.scale * x_scale_, ppu_b = b.scale * x_scale_;
            // Screen height of the rail at each end (y decreases upward).
            const float ha = h1 * ppu_a, hb = h2 * ppu_b;
            const float fog_amount = 1.f - s.fog;
            auto fogc = [&](Color c) { return fogged(c, fog_air_, fog_amount, daylight_); };
            const RoadTheme& th = track.look(s.index);
            const Color post_c = fogc(th.rail[1]);
            const Color bar_c = fogc(th.rail[0]);
            Color top_c = bar_c;
            top_c.r = static_cast<uint8_t>(std::min(255, top_c.r + 40));
            top_c.g = static_cast<uint8_t>(std::min(255, top_c.g + 40));
            top_c.b = static_cast<uint8_t>(std::min(255, top_c.b + 40));
            // Height fractions along the post (road = 0, top = 1), software bands.
            auto at = [&](float t, float r) {
                const float x = xa + (xb - xa) * t;
                const float base = a.y + (b.y - a.y) * t;
                const float h = ha + (hb - ha) * t;
                return std::pair<float, float>{x, base - h * r};
            };
            // Lower bar 0.20 .. 0.42
            {
                const auto n0 = at(0.f, 0.42f), n1 = at(0.f, 0.20f);
                const auto f0 = at(1.f, 0.42f), f1 = at(1.f, 0.20f);
                push_solid_quad(n0.first, n0.second, f0.first, f0.second, f1.first, f1.second, n1.first, n1.second,
                                bar_c);
            }
            // Upper bar 0.60 .. 0.95 (bright lip on the top edge via top_c on the upper half)
            {
                const auto n0 = at(0.f, 0.95f), n1 = at(0.f, 0.60f);
                const auto f0 = at(1.f, 0.95f), f1 = at(1.f, 0.60f);
                push_solid_quad(n0.first, n0.second, f0.first, f0.second, f1.first, f1.second, n1.first, n1.second,
                                bar_c);
                const auto lip_n0 = at(0.f, 0.95f), lip_n1 = at(0.f, 0.88f);
                const auto lip_f0 = at(1.f, 0.95f), lip_f1 = at(1.f, 0.88f);
                push_solid_quad(lip_n0.first, lip_n0.second, lip_f0.first, lip_f0.second, lip_f1.first, lip_f1.second,
                                lip_n1.first, lip_n1.second, top_c);
            }
            // Post at the near end of the segment (first ~10% of the run).
            {
                const float t0 = direction_ > 0 ? 0.f : 0.9f;
                const float t1 = direction_ > 0 ? 0.1f : 1.f;
                const auto n0 = at(t0, 1.f), n1 = at(t0, 0.f);
                const auto f0 = at(t1, 1.f), f1 = at(t1, 0.f);
                // Widen slightly in x so the post reads as a column.
                const float pw = std::max(1.5f, std::min(std::abs(xb - xa) * 0.08f, 4.f));
                const float sx = static_cast<float>(side);
                push_solid_quad(n0.first - sx * pw * 0.5f, n0.second, f0.first - sx * pw * 0.5f, f0.second,
                                f1.first + sx * pw * 0.5f, f1.second, n1.first + sx * pw * 0.5f, n1.second, post_c);
            }
        };
        if (s.p1.cam_z > camera_depth_) {
            draw_rail(-1);
            draw_rail(+1);
        }

        // Tunnel side walls at tunnel_half_width (matches mouth clip + collision).
        // Continuous bands near→far; opaque fill from road edge out so nothing
        // shows through past the walls.
        if (projectable && seg.tunnel) {
            const ScreenPoint& a = s.p1;
            const ScreenPoint& b = s.p2;
            const float fog_amount = 1.f - s.fog;
            auto fogc = [&](Color c) { return fogged(c, fog_air_, fog_amount, daylight_); };
            const Color kerb = fogc(Color{0xe0, 0xdc, 0xd4});
            const Color tile = fogc((s.index % 2) ? Color{0xc4, 0xbc, 0xb0} : Color{0xb8, 0xb0, 0xa4});
            const Color upper = fogc(Color{0x7c, 0x78, 0x70});
            const Color bulk = fogc(Color{0x6c, 0x68, 0x62}); // solid rock outside the face
            const float ha = tunnel_height * a.scale * y_scale_;
            const float hb = tunnel_height * b.scale * y_scale_;
            for (int side = -1; side <= 1; side += 2) {
                const float sd = static_cast<float>(side);
                // Wall face at tunnel_half_width road half-widths from centre.
                const float xa = a.x + sd * tunnel_half_width * a.w;
                const float xb = b.x + sd * tunnel_half_width * b.w;
                // Road edge (inner) — fill bulk between edge and wall so the
                // aperture cannot show scenery behind the tunnel.
                const float xa_in = a.x + sd * a.w;
                const float xb_in = b.x + sd * b.w;
                // Outer bulk to screen edge (blocks see-through past the wall).
                const float xa_out = sd < 0.f ? 0.f : static_cast<float>(width_);
                const float xb_out = xa_out;
                auto band = [&](float r0, float r1, Color c) {
                    const float ya0 = a.y - ha * r0, ya1 = a.y - ha * r1;
                    const float yb0 = b.y - hb * r0, yb1 = b.y - hb * r1;
                    push_solid_quad(xa, ya1, xb, yb1, xb, yb0, xa, ya0, c);
                };
                // Opaque sides: full height bulk outside the tunnel wall face.
                push_solid_quad(xa, a.y - ha, xb, b.y - hb, xb_out, b.y - hb, xa_out, a.y - ha, bulk);
                push_solid_quad(xa, a.y - ha, xb, b.y - hb, xb_out, b.y, xa_out, a.y, bulk);
                band(0.f, 0.14f, kerb);
                band(0.14f, 0.55f, tile);
                band(0.55f, 1.f, upper);
            }
        }

        // Cliff billboards (subsampled like software; nested strides so
        // columns never pop out as they approach).
        auto draw_cliff = [&](int side, const Bitmap& cliff) {
            const Edge kind = side < 0 ? seg.left : seg.right;
            if (kind != Edge::Cliff || cliff.w <= 0 || cliff.h <= 0) return;
            const float off = cliff_offset * static_cast<float>(side);
            const float xa = p0.x + off * p0.w;
            const int near = direction_ > 0 ? s.index : s.index + 1;
            const float h1 = track.edge_height(near, side);
            const float h2 = track.edge_height(near + direction_, side);
            if (!(h1 > 50.f) && !(h2 > 50.f)) return;
            const float ppu = std::max(p0.scale * x_scale_, 1e-4f);
            // Nested: far=4, near=2 (superset of far). See RoadRenderer::draw_edge.
            const int stride = ppu > 0.12f ? 2 : 4;
            const Edge prev_e = side < 0 ? track.segment(s.index - direction_).left
                                         : track.segment(s.index - direction_).right;
            const Edge next_e = side < 0 ? track.segment(s.index + direction_).left
                                         : track.segment(s.index + direction_).right;
            const bool run_end = prev_e != Edge::Cliff || next_e != Edge::Cliff;
            if ((s.index % stride) != 0 && !run_end) return;
            const float height = std::max(h1, h2 * 0.5f) * ppu;
            const float segs = static_cast<float>(stride) + 0.25f;
            const float width = std::max(track.segment_length * ppu * segs, height * 1.05f);
            if (!(height > 2.f) || !(width > 2.f)) return;
            const float left = side < 0 ? xa - width : xa;
            const uint8_t day = static_cast<uint8_t>(std::min(255.f, daylight_ * 255.f + 0.5f));
            const uint8_t fa = static_cast<uint8_t>(std::min(255.f, fog_amount * 255.f + 0.5f));
            Color tint{day, day, day, fa};
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
            const uint8_t day = static_cast<uint8_t>(std::min(255.f, daylight_ * 255.f + 0.5f));
            const uint8_t fa = static_cast<uint8_t>(std::min(255.f, fog_amount * 255.f + 0.5f));
            Color tint{day, day, day, fa};
            const unsigned tex = texture_for(bmp);
            if (!tex) return;
            flush_solid();
            push_quad(left, p0.y - height, width, height, 0.f, 0.f, 1.f, 1.f, tint, flip);
            flush_textured(tex);
            if (obj.kind == Scenery::StreetLamp) {
                // Pool centre sits a little toward the road from the pole base.
                const float toward_road = (obj.offset < 0.f ? 1.f : -1.f) * width * 0.15f;
                lamps_.push_back({left + width / 2.f + toward_road, p0.cam_z, 1100.f, Glow::Street});
            }
        };
        if (projectable) {
            const float shift = track.branch_offset(s.index);
            if (const Segment* other = track.other_route_segment(s.index); other && !std::isnan(shift)) {
                for (const RoadsideObject& obj : other->scenery) plant(obj, shift);
            }
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
                // Player composite: reused buffer, near plane (no fog).
                const uint8_t day = static_cast<uint8_t>(std::min(255.f, daylight_ * 255.f + 0.5f));
                Color tint{day, day, day, 0};
                const unsigned tex = texture_for(bmp, true);
                if (!tex) return;
                flush_solid();
                if (o.angle != 0.f) {
                    push_quad_rotated(o.sx + o.sw * 0.5f, o.sy + o.sh * 0.5f, o.sw, o.sh, o.angle, 0.f, 0.f, 1.f,
                                      1.f, tint);
                } else {
                    push_quad(o.sx, o.sy, o.sw, o.sh, 0.f, 0.f, 1.f, 1.f, tint, false);
                }
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
            const uint8_t day = static_cast<uint8_t>(std::min(255.f, daylight_ * 255.f + 0.5f));
            const uint8_t fa = static_cast<uint8_t>(std::min(255.f, fog_amount * 255.f + 0.5f));
            Color tint{day, day, day, fa};
            const unsigned tex = texture_for(bmp);
            if (!tex) return;
            flush_solid();
            push_quad(cx - width / 2.f, y - height, width, height, 0.f, 0.f, 1.f, 1.f, tint, o.flip);
            flush_textured(tex);
            if (o.lights != 0 && scale > 1e-4f) {
                // Interpolate cam_z at the object (same units as row_depth_), not
                // 1/scale which is noisier near the horizon and can "park" pools.
                const float depth = p0.cam_z + (p1.cam_z - p0.cam_z) * t;
                if (depth > camera_depth_ * 0.5f) {
                    const float ahead = static_cast<float>(o.lights);
                    lamps_.push_back({cx, depth + ahead * 1100.f, 1000.f, Glow::Head});
                    lamps_.push_back({cx, depth - ahead * 350.f, 560.f, Glow::Tail});
                }
            }
        };
        if (direction_ > 0) {
            for (auto o = std::make_reverse_iterator(last); o != std::make_reverse_iterator(first); ++o)
                draw_object(*o);
        } else {
            for (auto o = first; o != last; ++o) draw_object(*o);
        }

        flush_solid();
        if (use_scissor && g.Disable) g.Disable(GL_SCISSOR_TEST_);
    }
}



void GlesRenderer::draw_lamp_pools(float ambient) {
    if (lamps_.empty()) return;
    const float dark = 1.f - ambient;
    if (dark <= 0.02f) return;
    // Lightmap is cleared to ambient; add the remaining headroom toward the tint
    // so albedo × lightmap restores day colour under the lamp (software mix).
    g.BlendFunc(GL_ONE_, GL_ONE_);
    for (const LampSpot& lamp : lamps_) {
        const float tint_r = 1.f;
        const float tint_g = lamp.glow == Glow::Street ? 0.85f : lamp.glow == Glow::Tail ? 0.35f : 1.f;
        const float tint_b = lamp.glow == Glow::Street ? 0.55f : lamp.glow == Glow::Tail ? 0.2f : 0.95f;
        const float strength = lamp.glow == Glow::Tail ? 0.85f : lamp.glow == Glow::Head ? 0.8f : 0.9f;
        for (int y = 0; y < height_; ++y) {
            const float depth = row_depth_[static_cast<size_t>(y)];
            if (depth <= 0.f) continue;
            const float dz = depth - lamp.depth;
            if (std::abs(dz) >= lamp.reach) continue;
            const float px_per_unit = camera_depth_ / depth * x_scale_;
            const float half = std::sqrt(lamp.reach * lamp.reach - dz * dz) * px_per_unit;
            if (half < 0.5f) continue;
            // Concentric rings approximate radial (1−r²)² falloff.
            static constexpr float frac[4] = {1.f, 0.72f, 0.45f, 0.2f};
            static constexpr float wgt[4] = {0.15f, 0.3f, 0.45f, 0.7f};
            for (int ring = 0; ring < 4; ++ring) {
                const float edge = frac[ring];
                const float r_lat = 1.f - edge;
                const float r2 = (dz * dz) / (lamp.reach * lamp.reach) + r_lat * r_lat * 0.5f;
                if (r2 >= 1.f) continue;
                const float k = strength * dark * (1.f - r2) * (1.f - r2) * wgt[ring];
                if (k < 0.01f) continue;
                // Additive headroom toward tint (lightmap ends ≤ ~1).
                Color c{static_cast<uint8_t>(std::min(255.f, tint_r * k * 255.f)),
                        static_cast<uint8_t>(std::min(255.f, tint_g * k * 255.f)),
                        static_cast<uint8_t>(std::min(255.f, tint_b * k * 255.f)), 255};
                const float h = half * edge;
                push_trap(static_cast<float>(y), lamp.x - h, lamp.x + h, static_cast<float>(y + 1), lamp.x - h,
                          lamp.x + h, c);
            }
        }
    }
    flush_solid();
    g.BlendFunc(GL_SRC_ALPHA_, GL_ONE_MINUS_SRC_ALPHA_);
}


void GlesRenderer::draw_weather(const Weather& weather) {
    const int nr = weather.rain_count();
    const int ns = weather.snow_count();
    if (nr <= 0 && ns <= 0) return;
    constexpr float streak_seconds = 0.035f;
    // Rain streaks
    for (int i = 0; i < nr; ++i) {
        const Weather::Particle& p = weather.rain_data()[i];
        const float v = std::hypot(p.vx, p.vy);
        const float len = std::min(3.f + p.depth * 6.f + p.outflow * streak_seconds, 12.f + p.depth * 32.f);
        const float alpha = 0.3f + 0.4f * p.depth;
        const float ux = v > 1e-3f ? p.vx / v : 0.f, uy = v > 1e-3f ? p.vy / v : 1.f;
        const int steps = std::max(1, static_cast<int>(len));
        for (int k = 0; k < steps; ++k) {
            const float t = static_cast<float>(k);
            const float a = alpha * (1.f - 0.6f * t / len);
            Color c{0xc4, 0xd2, 0xe8, static_cast<uint8_t>(std::min(255.f, a * 255.f))};
            const float x = p.x - ux * t;
            const float y = p.y - uy * t;
            push_trap(y, x, x + 1.f, y + 1.f, x, x + 1.f, c);
        }
    }
    // Snow flakes / short streaks
    for (int i = 0; i < ns; ++i) {
        const Weather::Particle& p = weather.snow_data()[i];
        const float v = std::hypot(p.vx, p.vy);
        const float len = std::min(p.outflow * streak_seconds * 0.6f, 4.f + 10.f * p.depth);
        if (len > 1.5f && v > 1e-3f) {
            const float ux = p.vx / v, uy = p.vy / v;
            const float alpha = 0.55f + 0.45f * p.depth;
            for (int k = 0; k < static_cast<int>(len); ++k) {
                const float t = static_cast<float>(k);
                const float a = alpha * (1.f - 0.7f * t / len);
                Color c{255, 255, 255, static_cast<uint8_t>(std::min(255.f, a * 255.f))};
                const float x = p.x - ux * t;
                const float y = p.y - uy * t;
                push_trap(y, x, x + 1.f, y + 1.f, x, x + 1.f, c);
            }
        } else if (p.depth > 0.72f) {
            Color c{255, 255, 255, 242};
            push_trap(p.y, p.x, p.x + 2.f, p.y + 2.f, p.x, p.x + 2.f, c);
        } else {
            Color c{255, 255, 255, static_cast<uint8_t>((0.55f + 0.45f * p.depth) * 255.f)};
            push_trap(p.y, p.x, p.x + 1.f, p.y + 1.f, p.x, p.x + 1.f, c);
        }
    }
    flush_solid();
}

bool GlesRenderer::ensure_fbo() {
    if (fbo_ && light_fbo_ && fbo_w_ == width_ && fbo_h_ == height_) return true;
    if (depth_rb_ && g.DeleteRenderbuffers) g.DeleteRenderbuffers(1, &depth_rb_);
    if (color_tex_) g.DeleteTextures(1, &color_tex_);
    if (light_tex_) g.DeleteTextures(1, &light_tex_);
    if (light_fbo_ && g.DeleteFramebuffers) g.DeleteFramebuffers(1, &light_fbo_);
    if (fbo_ && g.DeleteFramebuffers) g.DeleteFramebuffers(1, &fbo_);
    fbo_ = color_tex_ = depth_rb_ = light_fbo_ = light_tex_ = 0;

    auto make_color_tex = [&](unsigned& tex) {
        g.GenTextures(1, &tex);
        g.BindTexture(GL_TEXTURE_2D_, tex);
        g.TexParameteri(GL_TEXTURE_2D_, GL_TEXTURE_MIN_FILTER_, GL_NEAREST_);
        g.TexParameteri(GL_TEXTURE_2D_, GL_TEXTURE_MAG_FILTER_, GL_NEAREST_);
        g.TexParameteri(GL_TEXTURE_2D_, GL_TEXTURE_WRAP_S_, GL_CLAMP_TO_EDGE_);
        g.TexParameteri(GL_TEXTURE_2D_, GL_TEXTURE_WRAP_T_, GL_CLAMP_TO_EDGE_);
        g.TexImage2D(GL_TEXTURE_2D_, 0, static_cast<int>(GL_RGBA_), width_, height_, 0, GL_RGBA_, GL_UNSIGNED_BYTE_,
                     nullptr);
    };

    g.GenFramebuffers(1, &fbo_);
    make_color_tex(color_tex_);
    g.GenRenderbuffers(1, &depth_rb_);
    g.BindRenderbuffer(GL_RENDERBUFFER_, depth_rb_);
    g.RenderbufferStorage(GL_RENDERBUFFER_, GL_DEPTH_COMPONENT16_, width_, height_);
    g.BindFramebuffer(GL_FRAMEBUFFER_, fbo_);
    g.FramebufferTexture2D(GL_FRAMEBUFFER_, GL_COLOR_ATTACHMENT0_, GL_TEXTURE_2D_, color_tex_, 0);
    g.FramebufferRenderbuffer(GL_FRAMEBUFFER_, GL_DEPTH_ATTACHMENT_, GL_RENDERBUFFER_, depth_rb_);
    if (g.CheckFramebufferStatus(GL_FRAMEBUFFER_) != GL_FRAMEBUFFER_COMPLETE_) {
        std::cerr << "kurvenrausch: gles color FBO incomplete\n";
        g.BindFramebuffer(GL_FRAMEBUFFER_, 0);
        return false;
    }

    // Lightmap: ambient + additive lamps; no depth.
    g.GenFramebuffers(1, &light_fbo_);
    make_color_tex(light_tex_);
    g.BindFramebuffer(GL_FRAMEBUFFER_, light_fbo_);
    g.FramebufferTexture2D(GL_FRAMEBUFFER_, GL_COLOR_ATTACHMENT0_, GL_TEXTURE_2D_, light_tex_, 0);
    if (g.CheckFramebufferStatus(GL_FRAMEBUFFER_) != GL_FRAMEBUFFER_COMPLETE_) {
        std::cerr << "kurvenrausch: gles light FBO incomplete\n";
        g.BindFramebuffer(GL_FRAMEBUFFER_, 0);
        return false;
    }
    g.BindFramebuffer(GL_FRAMEBUFFER_, 0);
    fbo_w_ = width_;
    fbo_h_ = height_;
    return true;
}

void GlesRenderer::draw_headlight(const Beam& beam, float ambient) {
    const float dark = 1.f - ambient;
    if (dark <= 0.01f) return;
    g.BlendFunc(GL_ONE_, GL_ONE_);
    auto smoothstep = [](float a, float b, float x) {
        const float t = std::clamp((x - a) / (b - a), 0.f, 1.f);
        return t * t * (3.f - 2.f * t);
    };
    int horizon_y = -1;
    float horizon_depth = 0.f;
    for (int y = 0; y < height_; ++y) {
        if (row_depth_[static_cast<size_t>(y)] > 0.f) {
            horizon_y = y;
            horizon_depth = row_depth_[static_cast<size_t>(y)];
            break;
        }
    }
    for (int y = 0; y < std::min(beam.bottom, height_); ++y) {
        float depth = row_depth_[static_cast<size_t>(y)];
        float air = 1.f;
        if (depth <= 0.f) {
            if (horizon_y < 0 || y >= horizon_y) continue;
            const float t = static_cast<float>(horizon_y - y) / static_cast<float>(std::max(1, horizon_y));
            depth = horizon_depth * (1.f + 1.8f * t);
            air = 0.55f * (1.f - 0.65f * t);
        }
        const float ahead = depth - beam.start;
        if (ahead <= 0.f || ahead > 4.f * beam_reach) continue;
        const float reach = smoothstep(0.f, 200.f, ahead) / (1.f + (ahead / beam_reach) * (ahead / beam_reach));
        const float px_per_unit = beam.camera_depth / depth * beam.x_scale;
        const float half = (beam_half_width + beam_spread * ahead) * px_per_unit;
        const float mid = beam.center + beam.aim * ahead * px_per_unit;
        const float k0 = 0.75f * dark * reach * air;
        if (k0 < 0.01f) continue;
        static constexpr float frac[4] = {1.f, 0.7f, 0.4f, 0.18f};
        static constexpr float wgt[4] = {0.12f, 0.28f, 0.45f, 0.65f};
        for (int ring = 0; ring < 4; ++ring) {
            const float edge = frac[ring];
            const float across = 1.f - edge;
            const float side = (1.f - across) * (1.f - across);
            const float k = k0 * side * wgt[ring];
            if (k < 0.01f) continue;
            Color c{static_cast<uint8_t>(std::min(255.f, k * 255.f)),
                    static_cast<uint8_t>(std::min(255.f, k * 245.f)),
                    static_cast<uint8_t>(std::min(255.f, k * 220.f)), 255};
            const float h = half * edge;
            push_trap(static_cast<float>(y), mid - h, mid + h, static_cast<float>(y + 1), mid - h, mid + h, c);
        }
    }
    flush_solid();
    g.BlendFunc(GL_SRC_ALPHA_, GL_ONE_MINUS_SRC_ALPHA_);
}


void GlesRenderer::render(const Track& track, const RoadView& view, const SpriteSheet& sprites,
                          std::vector<RoadSprite>& objects, const RoadTheme& theme, const Daylight& light,
                          const Background* backdrop, float hour, const Beam* headlight,
                          const Weather* weather) {
    if (!program_ || !ensure_fbo()) return;
    // Albedo at full day colour; night uses the same CPU path as software.
    daylight_ = 1.f;
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
            // Tunnel ceiling hides what lies beyond above it; mouths restrict the
            // horizontal aperture for farther slices (same as RoadRenderer).
            const float far_ceiling = s.p2.y - s.p2.scale * tunnel_height * y_scale_;
            if (seg.tunnel) ceiling = std::max(ceiling, far_ceiling);
            const bool way_in = seg.tunnel && !track.segment(index - dir).tunnel;
            const bool way_out = seg.tunnel && !track.segment(index + dir).tunnel;
            if (way_in || way_out) {
                const ScreenPoint& mouth = way_out ? s.p2 : s.p1;
                const float half = mouth.scale * tunnel_half_width * track.road_width * x_scale_;
                const float x0 = mouth.x - half, x1 = mouth.x + half;
                const float mouth_top = mouth.y - mouth.scale * tunnel_height * y_scale_;
                ceiling = std::max(ceiling, mouth_top);
                mouth_left = std::max(mouth_left, x0);
                mouth_right = std::min(mouth_right, x1);
            }
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

    // GL frame into the internal FBO
    Color air = fog_air_;
    if (theme.haze >= 0.99f || theme.fog_density >= 80.f) {
        air = theme.fog;
    }
    int prev_fbo = 0;
    if (g.GetIntegerv) g.GetIntegerv(GL_FRAMEBUFFER_BINDING_, &prev_fbo);
    g.BindFramebuffer(GL_FRAMEBUFFER_, fbo_);
    g.Viewport(0, 0, width_, height_);
    g.ClearColor(air.r / 255.f, air.g / 255.f, air.b / 255.f, 1.f);
    g.Clear(GL_COLOR_BUFFER_BIT_);
    g.Enable(GL_BLEND_);
    g.BlendFunc(GL_SRC_ALPHA_, GL_ONE_MINUS_SRC_ALPHA_);
    g.UseProgram(program_);
    g.Uniform2f(u_screen_, static_cast<float>(width_), static_cast<float>(height_));
    if (g.Uniform3f && u_fog_air_ >= 0) {
        g.Uniform3f(u_fog_air_, fog_air_.r / 255.f, fog_air_.g / 255.f, fog_air_.b / 255.f);
    }

    clear_batch();
    draw_backdrop(theme, backdrop, hour, horizon);
    for (const Slice& s : slices_) {
        if (!s.road_visible) continue;
        // Match software set_clip: tunnel mouth and nearer-road occlusion.
        const int clip_x0 = std::max(0, static_cast<int>(s.left));
        const int clip_x1 = std::min(width_, static_cast<int>(std::ceil(s.right)));
        const int clip_y0 = std::max(0, pixel_edge(s.top));
        const int clip_y1 = std::min(height_, clip_row(s.clip));
        const bool use_scissor = g.Scissor && g.Enable && clip_x1 > clip_x0 && clip_y1 > clip_y0 &&
                                 (clip_x0 > 0 || clip_x1 < width_ || clip_y0 > 0 || clip_y1 < height_);
        if (use_scissor) {
            flush_solid();
            g.Enable(GL_SCISSOR_TEST_);
            g.Scissor(clip_x0, height_ - clip_y1, clip_x1 - clip_x0, clip_y1 - clip_y0);
        }
        draw_segment(track, s, theme);
        if (use_scissor) {
            flush_solid();
            g.Disable(GL_SCISSOR_TEST_);
        }
    }
    flush_solid();

    // Ground snapshot (road/grass only) — software street_lights only relights
    // pixels the sprites did not cover (day[i] == ground[i]).
    std::vector<uint32_t> ground_argb;
    auto read_fbo_argb = [&](std::vector<uint32_t>& out) {
        out.resize(static_cast<size_t>(width_) * static_cast<size_t>(height_));
        if (!g.ReadPixels) {
            std::fill(out.begin(), out.end(), 0u);
            return;
        }
        std::vector<uint8_t> rgba(static_cast<size_t>(width_) * static_cast<size_t>(height_) * 4);
        g.ReadPixels(0, 0, width_, height_, GL_RGBA_, GL_UNSIGNED_BYTE_, rgba.data());
        // FBO origin is bottom-left; our y=0 is top.
        for (int y = 0; y < height_; ++y) {
            const int src_y = height_ - 1 - y;
            for (int x = 0; x < width_; ++x) {
                const size_t si = (static_cast<size_t>(src_y) * static_cast<size_t>(width_) + static_cast<size_t>(x)) * 4;
                const uint32_t r = rgba[si], gc = rgba[si + 1], b = rgba[si + 2], a = rgba[si + 3];
                out[static_cast<size_t>(y) * static_cast<size_t>(width_) + static_cast<size_t>(x)] =
                    (a << 24) | (r << 16) | (gc << 8) | b;
            }
        }
    };
    read_fbo_argb(ground_argb);

    draw_sprites(track, sprites, objects);
    flush_solid();
    if (weather) draw_weather(*weather);
    flush_solid();

    // Day picture (full-bright albedo + sprites + weather).
    // Skip the CPU night stack in full daylight with no beam.
    if (light.level < 0.999f || headlight || !lamps_.empty()) {
    std::vector<uint32_t> day_argb;
    read_fbo_argb(day_argb);

    // Same night stack as Game::render for the software path.
    Framebuffer night(width_, height_);
    std::copy(day_argb.begin(), day_argb.end(), night.pixels_mut());
    apply_daylight(night, light);
    street_lights(night, day_argb, ground_argb, light, row_depth_, lamps_, camera_depth_, x_scale_);
    if (headlight) headlight_beam(night, day_argb, light, row_depth_, *headlight);

    // Upload ARGB result back into the scene colour attachment (flip to GL origin).
    if (g.TexSubImage2D && color_tex_) {
        std::vector<uint8_t> rgba(static_cast<size_t>(width_) * static_cast<size_t>(height_) * 4);
        const uint32_t* src = night.pixels();
        for (int y = 0; y < height_; ++y) {
            const int dst_y = height_ - 1 - y;
            for (int x = 0; x < width_; ++x) {
                const uint32_t p = src[static_cast<size_t>(y) * static_cast<size_t>(width_) + static_cast<size_t>(x)];
                const size_t di = (static_cast<size_t>(dst_y) * static_cast<size_t>(width_) + static_cast<size_t>(x)) * 4;
                rgba[di + 0] = static_cast<uint8_t>((p >> 16) & 0xff);
                rgba[di + 1] = static_cast<uint8_t>((p >> 8) & 0xff);
                rgba[di + 2] = static_cast<uint8_t>(p & 0xff);
                rgba[di + 3] = static_cast<uint8_t>((p >> 24) & 0xff);
            }
        }
        g.BindTexture(GL_TEXTURE_2D_, color_tex_);
        g.PixelStorei(0x0CF5 /* GL_UNPACK_ALIGNMENT */, 1);
        g.TexSubImage2D(GL_TEXTURE_2D_, 0, 0, 0, width_, height_, GL_RGBA_, GL_UNSIGNED_BYTE_, rgba.data());
    }
    } // CPU night

    g.BindFramebuffer(GL_FRAMEBUFFER_, static_cast<unsigned>(prev_fbo));
}

} // namespace racer
