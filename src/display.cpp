// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "display.hpp"

#include "overlay.hpp"

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <iostream>
#include <string>
#include <type_traits>
#include <vector>

// GLES2 on the web, Android, and R36S/ArkOS (KURVEN_OPENGLES2); desktop GL 2.x
// otherwise. Entry points are resolved with SDL_GL_GetProcAddress so we do not
// depend on GLEW/glad or GL_GLEXT_PROTOTYPES (which many Linux toolchains leave
// undeclared). Types and enumerants used by the present path are defined below;
// the platform GL header is only needed where the toolchain ships one.
#if defined(__EMSCRIPTEN__) || defined(__ANDROID__)
#include <SDL2/SDL_opengles2.h>
#define KURVEN_GLES 1
#elif defined(KURVEN_OPENGLES2)
// Cross sysroots (ArkOS eoan) often lack GLES2/gl2platform.h that
// SDL_opengles2.h pulls in. KURVEN_GLES selects ES shaders + ES 2.0 context;
// no system GLES headers required.
#define KURVEN_GLES 1
#else
#include <SDL2/SDL_opengl.h>
#define KURVEN_GLES 0
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif
#endif

namespace racer {

namespace {

// Minimal GL 2 / ES 2 API surface used by the present path.
using GLenum = unsigned int;
using GLuint = unsigned int;
using GLint = int;
using GLsizei = int;
using GLfloat = float;
using GLboolean = unsigned char;
using GLchar = char;
using GLsizeiptr = std::ptrdiff_t;

constexpr GLenum GL_VERTEX_SHADER_ = 0x8B31;
constexpr GLenum GL_FRAGMENT_SHADER_ = 0x8B30;
constexpr GLenum GL_COMPILE_STATUS_ = 0x8B81;
constexpr GLenum GL_LINK_STATUS_ = 0x8B82;
constexpr GLenum GL_TEXTURE_2D_ = 0x0DE1;
constexpr GLenum GL_TEXTURE_MIN_FILTER_ = 0x2801;
constexpr GLenum GL_TEXTURE_MAG_FILTER_ = 0x2800;
constexpr GLenum GL_TEXTURE_WRAP_S_ = 0x2802;
constexpr GLenum GL_TEXTURE_WRAP_T_ = 0x2803;
constexpr GLenum GL_NEAREST_ = 0x2600;
constexpr GLenum GL_CLAMP_TO_EDGE_ = 0x812F;
constexpr GLenum GL_RGBA_ = 0x1908;
constexpr GLenum GL_BGRA_ = 0x80E1;
constexpr GLenum GL_UNSIGNED_BYTE_ = 0x1401;
constexpr GLenum GL_COLOR_BUFFER_BIT_ = 0x00004000;
constexpr GLenum GL_ARRAY_BUFFER_ = 0x8892;
constexpr GLenum GL_STREAM_DRAW_ = 0x88E0;
constexpr GLenum GL_FLOAT_ = 0x1406;
constexpr GLenum GL_TRIANGLE_STRIP_ = 0x0005;
constexpr GLenum GL_BLEND_ = 0x0BE2;
constexpr GLenum GL_SRC_ALPHA_ = 0x0302;
constexpr GLenum GL_ONE_MINUS_SRC_ALPHA_ = 0x0303;
constexpr GLenum GL_TEXTURE0_ = 0x84C0;
constexpr GLenum GL_FRAMEBUFFER_ = 0x8D40;
constexpr GLenum GL_COLOR_ATTACHMENT0_ = 0x8CE0;
constexpr GLboolean GL_FALSE_ = 0;

struct GlApi {
    GLuint (*CreateShader)(GLenum) = nullptr;
    void (*ShaderSource)(GLuint, GLsizei, const GLchar* const*, const GLint*) = nullptr;
    void (*CompileShader)(GLuint) = nullptr;
    void (*GetShaderiv)(GLuint, GLenum, GLint*) = nullptr;
    void (*GetShaderInfoLog)(GLuint, GLsizei, GLsizei*, GLchar*) = nullptr;
    void (*DeleteShader)(GLuint) = nullptr;
    GLuint (*CreateProgram)() = nullptr;
    void (*AttachShader)(GLuint, GLuint) = nullptr;
    void (*BindAttribLocation)(GLuint, GLuint, const GLchar*) = nullptr;
    void (*LinkProgram)(GLuint) = nullptr;
    void (*GetProgramiv)(GLuint, GLenum, GLint*) = nullptr;
    void (*GetProgramInfoLog)(GLuint, GLsizei, GLsizei*, GLchar*) = nullptr;
    void (*DeleteProgram)(GLuint) = nullptr;
    GLint (*GetUniformLocation)(GLuint, const GLchar*) = nullptr;
    void (*UseProgram)(GLuint) = nullptr;
    void (*Uniform1i)(GLint, GLint) = nullptr;
    void (*GenTextures)(GLsizei, GLuint*) = nullptr;
    void (*DeleteTextures)(GLsizei, const GLuint*) = nullptr;
    void (*BindTexture)(GLenum, GLuint) = nullptr;
    void (*TexParameteri)(GLenum, GLenum, GLint) = nullptr;
    void (*TexImage2D)(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum, GLenum, const void*) = nullptr;
    void (*TexSubImage2D)(GLenum, GLint, GLint, GLint, GLsizei, GLsizei, GLenum, GLenum, const void*) = nullptr;
    void (*GenBuffers)(GLsizei, GLuint*) = nullptr;
    void (*DeleteBuffers)(GLsizei, const GLuint*) = nullptr;
    void (*BindBuffer)(GLenum, GLuint) = nullptr;
    void (*BufferData)(GLenum, GLsizeiptr, const void*, GLenum) = nullptr;
    void (*EnableVertexAttribArray)(GLuint) = nullptr;
    void (*DisableVertexAttribArray)(GLuint) = nullptr;
    void (*VertexAttribPointer)(GLuint, GLint, GLenum, GLboolean, GLsizei, const void*) = nullptr;
    void (*DrawArrays)(GLenum, GLint, GLsizei) = nullptr;
    void (*Viewport)(GLint, GLint, GLsizei, GLsizei) = nullptr;
    void (*ClearColor)(GLfloat, GLfloat, GLfloat, GLfloat) = nullptr;
    void (*Clear)(GLenum) = nullptr;
    void (*ActiveTexture)(GLenum) = nullptr;
    void (*Enable)(GLenum) = nullptr;
    void (*Disable)(GLenum) = nullptr;
    void (*BlendFunc)(GLenum, GLenum) = nullptr;
    void (*GenFramebuffers)(GLsizei, GLuint*) = nullptr;
    void (*DeleteFramebuffers)(GLsizei, const GLuint*) = nullptr;
    void (*BindFramebuffer)(GLenum, GLuint) = nullptr;
    void (*FramebufferTexture2D)(GLenum, GLenum, GLenum, GLuint, GLint) = nullptr;

    bool load() {
        auto get = [](const char* name) { return SDL_GL_GetProcAddress(name); };
        auto need = [&](auto& fn, const char* name) {
            fn = reinterpret_cast<std::decay_t<decltype(fn)>>(get(name));
            if (!fn) {
                std::cerr << "kurvenrausch: missing GL entry " << name << "\n";
                return false;
            }
            return true;
        };
        if (!(need(CreateShader, "glCreateShader") && need(ShaderSource, "glShaderSource") &&
              need(CompileShader, "glCompileShader") && need(GetShaderiv, "glGetShaderiv") &&
              need(GetShaderInfoLog, "glGetShaderInfoLog") && need(DeleteShader, "glDeleteShader") &&
              need(CreateProgram, "glCreateProgram") && need(AttachShader, "glAttachShader") &&
              need(BindAttribLocation, "glBindAttribLocation") && need(LinkProgram, "glLinkProgram") &&
              need(GetProgramiv, "glGetProgramiv") && need(GetProgramInfoLog, "glGetProgramInfoLog") &&
              need(DeleteProgram, "glDeleteProgram") && need(GetUniformLocation, "glGetUniformLocation") &&
              need(UseProgram, "glUseProgram") && need(Uniform1i, "glUniform1i") &&
              need(GenTextures, "glGenTextures") && need(DeleteTextures, "glDeleteTextures") &&
              need(BindTexture, "glBindTexture") && need(TexParameteri, "glTexParameteri") &&
              need(TexImage2D, "glTexImage2D") && need(TexSubImage2D, "glTexSubImage2D") &&
              need(GenBuffers, "glGenBuffers") && need(DeleteBuffers, "glDeleteBuffers") &&
              need(BindBuffer, "glBindBuffer") && need(BufferData, "glBufferData") &&
              need(EnableVertexAttribArray, "glEnableVertexAttribArray") &&
              need(DisableVertexAttribArray, "glDisableVertexAttribArray") &&
              need(VertexAttribPointer, "glVertexAttribPointer") && need(DrawArrays, "glDrawArrays") &&
              need(Viewport, "glViewport") && need(ClearColor, "glClearColor") && need(Clear, "glClear") &&
              need(ActiveTexture, "glActiveTexture") && need(Enable, "glEnable") &&
              need(Disable, "glDisable") && need(BlendFunc, "glBlendFunc")))
            return false;
        auto soft = [&](auto& fn, const char* a, const char* b) {
            fn = reinterpret_cast<std::decay_t<decltype(fn)>>(get(a));
            if (!fn && b) fn = reinterpret_cast<std::decay_t<decltype(fn)>>(get(b));
        };
        soft(GenFramebuffers, "glGenFramebuffers", "glGenFramebuffersOES");
        soft(DeleteFramebuffers, "glDeleteFramebuffers", "glDeleteFramebuffersOES");
        soft(BindFramebuffer, "glBindFramebuffer", "glBindFramebufferOES");
        soft(FramebufferTexture2D, "glFramebufferTexture2D", "glFramebufferTexture2DOES");
        return true;
    }
};

GlApi g_gl;

const char* k_vert =
#if KURVEN_GLES
    "attribute vec2 a_pos;\n"
    "attribute vec2 a_uv;\n"
    "varying vec2 v_uv;\n"
    "void main(){ v_uv = a_uv; gl_Position = vec4(a_pos, 0.0, 1.0); }\n";
#else
    "#version 110\n"
    "attribute vec2 a_pos;\n"
    "attribute vec2 a_uv;\n"
    "varying vec2 v_uv;\n"
    "void main(){ v_uv = a_uv; gl_Position = vec4(a_pos, 0.0, 1.0); }\n";
#endif

const char* k_frag =
#if KURVEN_GLES
    "precision mediump float;\n"
    "varying vec2 v_uv;\n"
    "uniform sampler2D u_tex;\n"
    "uniform int u_swizzle_rb;\n"
    "void main(){\n"
    "  vec4 t = texture2D(u_tex, v_uv);\n"
    "  if (u_swizzle_rb != 0) gl_FragColor = vec4(t.b, t.g, t.r, t.a);\n"
    "  else gl_FragColor = t;\n"
    "}\n";
#else
    "#version 110\n"
    "varying vec2 v_uv;\n"
    "uniform sampler2D u_tex;\n"
    "uniform int u_swizzle_rb;\n"
    "void main(){\n"
    "  vec4 t = texture2D(u_tex, v_uv);\n"
    "  if (u_swizzle_rb != 0) gl_FragColor = vec4(t.b, t.g, t.r, t.a);\n"
    "  else gl_FragColor = t;\n"
    "}\n";
#endif

unsigned compile_shader(unsigned type, const char* src) {
    const unsigned s = g_gl.CreateShader(type);
    g_gl.ShaderSource(s, 1, &src, nullptr);
    g_gl.CompileShader(s);
    int ok = 0;
    g_gl.GetShaderiv(s, GL_COMPILE_STATUS_, &ok);
    if (!ok) {
        char log[256];
        g_gl.GetShaderInfoLog(s, sizeof log, nullptr, log);
        std::cerr << "kurvenrausch: shader: " << log << "\n";
        g_gl.DeleteShader(s);
        return 0;
    }
    return s;
}

unsigned link_program(unsigned vs, unsigned fs) {
    const unsigned p = g_gl.CreateProgram();
    g_gl.AttachShader(p, vs);
    g_gl.AttachShader(p, fs);
    g_gl.BindAttribLocation(p, 0, "a_pos");
    g_gl.BindAttribLocation(p, 1, "a_uv");
    g_gl.LinkProgram(p);
    int ok = 0;
    g_gl.GetProgramiv(p, GL_LINK_STATUS_, &ok);
    if (!ok) {
        char log[256];
        g_gl.GetProgramInfoLog(p, sizeof log, nullptr, log);
        std::cerr << "kurvenrausch: program: " << log << "\n";
        g_gl.DeleteProgram(p);
        return 0;
    }
    return p;
}

} // namespace

const char* scene_backend_name(SceneBackend b) {
    switch (b) {
        case SceneBackend::Software: return "SOFTWARE";
        case SceneBackend::Gles: return "GLES";
        case SceneBackend::Auto:
        default: return "AUTO";
    }
}

SceneBackend next_scene_backend(SceneBackend b) {
    // Hotkey toggles the two concrete backends only (not Auto).
    return b == SceneBackend::Gles ? SceneBackend::Software : SceneBackend::Gles;
}

bool parse_scene_backend(const char* s, SceneBackend& out) {
    if (!s) return false;
    std::string v(s);
    for (char& c : v) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    if (v == "auto") { out = SceneBackend::Auto; return true; }
    if (v == "software" || v == "sw" || v == "cpu") { out = SceneBackend::Software; return true; }
    if (v == "gles" || v == "gl" || v == "gpu") { out = SceneBackend::Gles; return true; }
    return false;
}

const char* present_backend_name(PresentBackend b) {
    switch (b) {
        case PresentBackend::Auto: return "AUTO";
        case PresentBackend::Sdl: return "SDL";
        case PresentBackend::Gl: return "GL";
    }
    return "SDL";
}

PresentBackend next_present_backend(PresentBackend b) {
    switch (b) {
        case PresentBackend::Auto: return PresentBackend::Sdl;
        case PresentBackend::Sdl: return PresentBackend::Gl;
        case PresentBackend::Gl: return PresentBackend::Auto;
    }
    return PresentBackend::Auto;
}

Display::~Display() {
    destroy_present();
    if (window_) {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }
    // Do not QuitSubSystem(VIDEO) here: other code (or SDL itself) may still
    // touch the display connection during process teardown, which on X11 shows
    // up as BadWindow / X_TranslateCoords. SDL_Quit at process exit is enough.
}

void Display::destroy_present() {
    for (const auto& [key, texture] : overlay_textures_) SDL_DestroyTexture(texture);
    overlay_textures_.clear();
    if (texture_) {
        SDL_DestroyTexture(texture_);
        texture_ = nullptr;
    }
    if (renderer_) {
        SDL_DestroyRenderer(renderer_);
        renderer_ = nullptr;
    }
    // GL objects must be deleted while the context is current; otherwise X11
    // can raise BadWindow during later TranslateCoords from SDL teardown.
    if (gl_ && window_) {
        SDL_GL_MakeCurrent(window_, gl_);
        if (gl_hud_fbo_ && g_gl.DeleteFramebuffers) {
            g_gl.DeleteFramebuffers(1, &gl_hud_fbo_);
            gl_hud_fbo_ = 0;
        }
        if (gl_fb_tex_ && g_gl.DeleteTextures) {
            g_gl.DeleteTextures(1, &gl_fb_tex_);
            gl_fb_tex_ = 0;
        }
        if (gl_vbo_ && g_gl.DeleteBuffers) {
            g_gl.DeleteBuffers(1, &gl_vbo_);
            gl_vbo_ = 0;
        }
        if (g_gl.DeleteTextures) {
            for (auto& kv : gl_overlay_tex_) {
                if (kv.second) g_gl.DeleteTextures(1, &kv.second);
            }
        }
        gl_overlay_tex_.clear();
        if (gl_program_ && g_gl.DeleteProgram) {
            g_gl.DeleteProgram(gl_program_);
            gl_program_ = 0;
        }
        SDL_GL_MakeCurrent(window_, nullptr);
        SDL_GL_DeleteContext(gl_);
        gl_ = nullptr;
    } else {
        gl_fb_tex_ = 0;
        gl_vbo_ = 0;
        gl_program_ = 0;
        if (gl_) {
            SDL_GL_DeleteContext(gl_);
            gl_ = nullptr;
        }
    }
}

bool Display::init(const char* title, const char* app_id, int fb_width, int fb_height, int window_scale,
                   bool fullscreen, PresentBackend preferred) {
    fb_w_ = fb_width;
    fb_h_ = fb_height;
    window_scale_ = window_scale;
    fullscreen_wanted_ = fullscreen;
    preferred_ = preferred;
    title_ = title ? title : "Kurvenrausch";

#ifdef SDL_HINT_VIDEO_X11_WMCLASS
    SDL_SetHint(SDL_HINT_VIDEO_X11_WMCLASS, app_id);
#endif
#ifdef SDL_HINT_VIDEO_WAYLAND_WMCLASS
    SDL_SetHint(SDL_HINT_VIDEO_WAYLAND_WMCLASS, app_id);
#endif
#ifdef SDL_HINT_APP_NAME
    SDL_SetHint(SDL_HINT_APP_NAME, title);
#endif
    (void)app_id;

#ifdef __ANDROID__
    SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");
#endif
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
        std::cerr << "SDL video init failed: " << SDL_GetError() << "\n";
        return false;
    }

    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
#if KURVEN_GLES
    // Prefer the EGL/GLES driver on Mali (ArkOS / R36S). Without this, SDL may
    // try a desktop GL path that never creates a context. SuperTux Origins uses
    // the same hint for SUPERTUX_R36S.
#ifdef SDL_HINT_OPENGL_ES_DRIVER
    SDL_SetHint(SDL_HINT_OPENGL_ES_DRIVER, "1");
#endif
#endif

    Uint32 flags = SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_OPENGL;
    if (fullscreen) flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;

    window_ = SDL_CreateWindow(title_.c_str(), SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                               fb_width * window_scale, fb_height * window_scale, flags);
    if (!window_) {
        std::cerr << "Window creation failed: " << SDL_GetError() << "\n";
        return false;
    }

    return rebuild_present(preferred);
}

bool Display::rebuild_present(PresentBackend preferred) {
    destroy_present();
    preferred_ = preferred;

    const auto try_gl = [&] {
        if (init_gl_present()) {
            backend_ = PresentBackend::Gl;
            return true;
        }
        destroy_present();
        return false;
    };
    const auto try_sdl = [&] {
        if (init_sdl_present()) {
            backend_ = PresentBackend::Sdl;
            return true;
        }
        destroy_present();
        return false;
    };

    switch (preferred) {
        case PresentBackend::Gl:
            if (try_gl()) return true;
            std::cerr << "kurvenrausch: GL present failed, falling back to SDL\n";
            return try_sdl();
        case PresentBackend::Sdl:
            return try_sdl();
        case PresentBackend::Auto:
        default:
            if (try_gl()) return true;
            return try_sdl();
    }
}

bool Display::set_present_backend(PresentBackend preferred) {
    if (!window_) return false;
    const PresentBackend was = backend_;
    if (!rebuild_present(preferred)) {
        // Last resort: try to restore the previous backend.
        rebuild_present(was == PresentBackend::Gl ? PresentBackend::Gl : PresentBackend::Sdl);
        return false;
    }
    return true;
}

bool Display::init_sdl_present() {
    renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer_) {
        std::cerr << "Accelerated renderer unavailable (" << SDL_GetError()
                  << "), falling back to software\n";
        renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_SOFTWARE);
    }
    if (!renderer_) {
        std::cerr << "Renderer creation failed: " << SDL_GetError() << "\n";
        return false;
    }

    SDL_RendererInfo info{};
    if (SDL_GetRendererInfo(renderer_, &info) == 0)
        vsync_ = (info.flags & SDL_RENDERER_PRESENTVSYNC) != 0;

    // STREAMING + lock is cheaper than UpdateTexture on several drivers.
    texture_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, fb_w_, fb_h_);
    if (!texture_) {
        std::cerr << "Texture creation failed: " << SDL_GetError() << "\n";
        return false;
    }
#ifdef SDL_VERSION_ATLEAST
#if SDL_VERSION_ATLEAST(2, 0, 12)
    SDL_SetTextureScaleMode(texture_, SDL_ScaleModeNearest);
#endif
#endif
    return true;
}

bool Display::init_gl_present() {
#if KURVEN_GLES
    // GLES stacks (Android, R36S, WebGL): request ES 2.0 only. Do not force
    // colour-buffer sizes — hand-tuned RGBA sizes have caused "Could not
    // create EGL window surface" on ArkOS (SuperTux Origins PORTING.md).
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#else
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
#endif
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    gl_ = SDL_GL_CreateContext(window_);
    if (!gl_) {
        std::cerr << "GL context failed: " << SDL_GetError() << "\n";
        return false;
    }
    SDL_GL_MakeCurrent(window_, gl_);
    if (!g_gl.load()) return false;
    SDL_GL_SetSwapInterval(1);
    vsync_ = true;

    const unsigned vs = compile_shader(GL_VERTEX_SHADER_, k_vert);
    const unsigned fs = compile_shader(GL_FRAGMENT_SHADER_, k_frag);
    if (!vs || !fs) {
        if (vs) g_gl.DeleteShader(vs);
        if (fs) g_gl.DeleteShader(fs);
        return false;
    }
    gl_program_ = link_program(vs, fs);
    g_gl.DeleteShader(vs);
    g_gl.DeleteShader(fs);
    if (!gl_program_) return false;
    gl_u_tex_ = g_gl.GetUniformLocation(gl_program_, "u_tex");
    gl_u_swizzle_ = g_gl.GetUniformLocation(gl_program_, "u_swizzle_rb");

    g_gl.GenTextures(1, &gl_fb_tex_);
    g_gl.BindTexture(GL_TEXTURE_2D_, gl_fb_tex_);
    g_gl.TexParameteri(GL_TEXTURE_2D_, GL_TEXTURE_MIN_FILTER_, GL_NEAREST_);
    g_gl.TexParameteri(GL_TEXTURE_2D_, GL_TEXTURE_MAG_FILTER_, GL_NEAREST_);
    g_gl.TexParameteri(GL_TEXTURE_2D_, GL_TEXTURE_WRAP_S_, GL_CLAMP_TO_EDGE_);
    g_gl.TexParameteri(GL_TEXTURE_2D_, GL_TEXTURE_WRAP_T_, GL_CLAMP_TO_EDGE_);
    g_gl.TexImage2D(GL_TEXTURE_2D_, 0, static_cast<GLint>(GL_RGBA_), fb_w_, fb_h_, 0, GL_RGBA_,
                    GL_UNSIGNED_BYTE_, nullptr);

    g_gl.GenBuffers(1, &gl_vbo_);
    return true;
}

void Display::set_icon(const uint32_t* argb_pixels, int width, int height) {
    if (!window_ || !argb_pixels || width <= 0 || height <= 0) return;
    SDL_Surface* surface =
        SDL_CreateRGBSurfaceFrom(const_cast<uint32_t*>(argb_pixels), width, height, 32, width * 4,
                                 0x00ff0000u, 0x0000ff00u, 0x000000ffu, 0xff000000u);
    if (!surface) return;
    SDL_SetWindowIcon(window_, surface);
    SDL_FreeSurface(surface);
}

bool Display::resize_framebuffer(int fb_width, int fb_height) {
    if (fb_width <= 0 || fb_height <= 0) return false;
    if (fb_width == fb_w_ && fb_height == fb_h_) return true;
    fb_w_ = fb_width;
    fb_h_ = fb_height;
    return rebuild_present(preferred_);
}

void Display::toggle_fullscreen() {
    SDL_SetWindowFullscreen(window_, is_fullscreen() ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP);
}

bool Display::is_fullscreen() const {
    return window_ && (SDL_GetWindowFlags(window_) & SDL_WINDOW_FULLSCREEN_DESKTOP) != 0;
}

SDL_Rect Display::screen() const {
    int w = 0, h = 0;
    if (backend_ == PresentBackend::Gl) {
        SDL_GL_GetDrawableSize(window_, &w, &h);
    } else if (renderer_) {
        if (SDL_GetRendererOutputSize(renderer_, &w, &h) != 0 || w <= 0 || h <= 0)
            SDL_GetWindowSize(window_, &w, &h);
    } else {
        SDL_GetWindowSize(window_, &w, &h);
    }
    return SDL_Rect{0, 0, std::max(w, 1), std::max(h, 1)};
}

SDL_Rect Display::picture() const {
    const SDL_Rect s = screen();
    const float scale = std::min(static_cast<float>(s.w) / static_cast<float>(fb_w_),
                                 static_cast<float>(s.h) / static_cast<float>(fb_h_));
    const int w = static_cast<int>(static_cast<float>(fb_w_) * scale);
    const int h = static_cast<int>(static_cast<float>(fb_h_) * scale);
    return SDL_Rect{(s.w - w) / 2, (s.h - h) / 2, w, h};
}

void Display::touch_to_screen(float tx, float ty, float& x, float& y) const {
    const SDL_Rect s = screen();
    x = tx * static_cast<float>(s.w);
    y = ty * static_cast<float>(s.h);
}

void Display::screen_to_framebuffer(float sx, float sy, float& x, float& y) const {
    const SDL_Rect pic = picture();
    x = (sx - static_cast<float>(pic.x)) * static_cast<float>(fb_w_) / static_cast<float>(pic.w);
    y = (sy - static_cast<float>(pic.y)) * static_cast<float>(fb_h_) / static_cast<float>(pic.h);
}

void Display::present(const uint32_t* argb_pixels, const Overlay& overlay) {
    if (backend_ == PresentBackend::Gl) present_gl(argb_pixels, overlay);
    else present_sdl(argb_pixels, overlay);
}

void Display::present_sdl(const uint32_t* argb_pixels, const Overlay& overlay) {
    void* pixels = nullptr;
    int pitch = 0;
    if (SDL_LockTexture(texture_, nullptr, &pixels, &pitch) == 0) {
        const int row_bytes = fb_w_ * static_cast<int>(sizeof(uint32_t));
        if (pitch == row_bytes) {
            std::memcpy(pixels, argb_pixels, static_cast<size_t>(row_bytes * fb_h_));
        } else {
            auto* dst = static_cast<uint8_t*>(pixels);
            const auto* src = reinterpret_cast<const uint8_t*>(argb_pixels);
            for (int y = 0; y < fb_h_; ++y)
                std::memcpy(dst + y * pitch, src + y * row_bytes, static_cast<size_t>(row_bytes));
        }
        SDL_UnlockTexture(texture_);
    } else {
        SDL_UpdateTexture(texture_, nullptr, argb_pixels, fb_w_ * static_cast<int>(sizeof(uint32_t)));
    }

    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
    SDL_RenderClear(renderer_);
    const SDL_Rect pic = picture();
    SDL_RenderCopy(renderer_, texture_, nullptr, &pic);

    if (overlay_textures_.size() > 128) {
        for (const auto& [key, texture] : overlay_textures_) SDL_DestroyTexture(texture);
        overlay_textures_.clear();
    }
    for (const Overlay::Item& item : overlay.items()) {
        SDL_Texture*& texture = overlay_textures_[*item.key];
        if (!texture) {
            texture = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC,
                                        item.image->w, item.image->h);
            if (!texture) continue;
            SDL_UpdateTexture(texture, nullptr, item.image->px.data(),
                              item.image->w * static_cast<int>(sizeof(uint32_t)));
            SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);
        }
        const SDL_Rect dst{item.x, item.y, item.image->w, item.image->h};
        SDL_RenderCopy(renderer_, texture, nullptr, &dst);
    }
    SDL_RenderPresent(renderer_);
}

void Display::present_gl(const uint32_t* argb_pixels, const Overlay& overlay) {
    SDL_GL_MakeCurrent(window_, gl_);
    const SDL_Rect s = screen();
    const SDL_Rect pic = picture();
    g_gl.Viewport(0, 0, s.w, s.h);
    g_gl.ClearColor(0.f, 0.f, 0.f, 1.f);
    g_gl.Clear(GL_COLOR_BUFFER_BIT_);

    g_gl.BindTexture(GL_TEXTURE_2D_, gl_fb_tex_);
#if KURVEN_GLES
    // Upload ARGB bytes as RGBA; shader swaps R/B (avoids full-buffer CPU swizzle).
    g_gl.TexSubImage2D(GL_TEXTURE_2D_, 0, 0, 0, fb_w_, fb_h_, GL_RGBA_, GL_UNSIGNED_BYTE_, argb_pixels);
#else
    g_gl.TexSubImage2D(GL_TEXTURE_2D_, 0, 0, 0, fb_w_, fb_h_, GL_BGRA_, GL_UNSIGNED_BYTE_, argb_pixels);
#endif

    g_gl.UseProgram(gl_program_);
    g_gl.Uniform1i(gl_u_tex_, 0);
    if (gl_u_swizzle_ >= 0) g_gl.Uniform1i(gl_u_swizzle_, 1); // CPU ARGB as RGBA
    g_gl.ActiveTexture(GL_TEXTURE0_);
    g_gl.BindTexture(GL_TEXTURE_2D_, gl_fb_tex_);

    const float x0 = 2.f * static_cast<float>(pic.x) / static_cast<float>(s.w) - 1.f;
    const float x1 = 2.f * static_cast<float>(pic.x + pic.w) / static_cast<float>(s.w) - 1.f;
    const float y0 = 1.f - 2.f * static_cast<float>(pic.y + pic.h) / static_cast<float>(s.h);
    const float y1 = 1.f - 2.f * static_cast<float>(pic.y) / static_cast<float>(s.h);
    const float verts[] = {
        x0, y0, 0.f, 1.f, x1, y0, 1.f, 1.f, x0, y1, 0.f, 0.f, x1, y1, 1.f, 0.f,
    };
    g_gl.BindBuffer(GL_ARRAY_BUFFER_, gl_vbo_);
    g_gl.BufferData(GL_ARRAY_BUFFER_, static_cast<GLsizeiptr>(sizeof verts), verts, GL_STREAM_DRAW_);
    g_gl.EnableVertexAttribArray(0);
    g_gl.EnableVertexAttribArray(1);
    g_gl.VertexAttribPointer(0, 2, GL_FLOAT_, GL_FALSE_, 4 * static_cast<GLsizei>(sizeof(float)),
                             reinterpret_cast<void*>(0));
    g_gl.VertexAttribPointer(1, 2, GL_FLOAT_, GL_FALSE_, 4 * static_cast<GLsizei>(sizeof(float)),
                             reinterpret_cast<void*>(2 * sizeof(float)));
    g_gl.DrawArrays(GL_TRIANGLE_STRIP_, 0, 4);

    g_gl.Enable(GL_BLEND_);
    g_gl.BlendFunc(GL_SRC_ALPHA_, GL_ONE_MINUS_SRC_ALPHA_);
    if (gl_u_swizzle_ >= 0) g_gl.Uniform1i(gl_u_swizzle_, 1); // overlay bitmaps are ARGB
    for (const Overlay::Item& item : overlay.items()) {
        if (!item.image || item.image->w <= 0 || item.image->h <= 0 || item.image->px.empty()) continue;
        const void* key = item.image->px.data();
        unsigned tex = 0;
        if (auto it = gl_overlay_tex_.find(key); it != gl_overlay_tex_.end()) {
            tex = it->second;
        } else {
            g_gl.GenTextures(1, &tex);
            g_gl.BindTexture(GL_TEXTURE_2D_, tex);
            g_gl.TexParameteri(GL_TEXTURE_2D_, GL_TEXTURE_MIN_FILTER_, GL_NEAREST_);
            g_gl.TexParameteri(GL_TEXTURE_2D_, GL_TEXTURE_MAG_FILTER_, GL_NEAREST_);
            g_gl.TexParameteri(GL_TEXTURE_2D_, GL_TEXTURE_WRAP_S_, GL_CLAMP_TO_EDGE_);
            g_gl.TexParameteri(GL_TEXTURE_2D_, GL_TEXTURE_WRAP_T_, GL_CLAMP_TO_EDGE_);
#if KURVEN_GLES
            g_gl.TexImage2D(GL_TEXTURE_2D_, 0, static_cast<GLint>(GL_RGBA_), item.image->w, item.image->h, 0, GL_RGBA_,
                            GL_UNSIGNED_BYTE_, item.image->px.data());
#else
            g_gl.TexImage2D(GL_TEXTURE_2D_, 0, static_cast<GLint>(GL_RGBA_), item.image->w, item.image->h, 0, GL_BGRA_,
                            GL_UNSIGNED_BYTE_, item.image->px.data());
#endif
            gl_overlay_tex_[key] = tex;
        }
        g_gl.BindTexture(GL_TEXTURE_2D_, tex);
        const float ox0 = 2.f * static_cast<float>(item.x) / static_cast<float>(s.w) - 1.f;
        const float ox1 = 2.f * static_cast<float>(item.x + item.image->w) / static_cast<float>(s.w) - 1.f;
        const float oy0 = 1.f - 2.f * static_cast<float>(item.y + item.image->h) / static_cast<float>(s.h);
        const float oy1 = 1.f - 2.f * static_cast<float>(item.y) / static_cast<float>(s.h);
        const float ov[] = {
            ox0, oy0, 0.f, 1.f, ox1, oy0, 1.f, 1.f, ox0, oy1, 0.f, 0.f, ox1, oy1, 1.f, 0.f,
        };
        g_gl.BufferData(GL_ARRAY_BUFFER_, static_cast<GLsizeiptr>(sizeof ov), ov, GL_STREAM_DRAW_);
        g_gl.DrawArrays(GL_TRIANGLE_STRIP_, 0, 4);
    }
    g_gl.Disable(GL_BLEND_);

    g_gl.DisableVertexAttribArray(0);
    g_gl.DisableVertexAttribArray(1);
    SDL_GL_SwapWindow(window_);
}




void Display::present_gles_scene(unsigned scene_tex, int tex_w, int tex_h, const uint32_t* hud_argb,
                                 const Overlay& overlay) {
    if (!is_gl() || !window_ || !gl_program_ || !scene_tex) {
        present_overlay(overlay);
        return;
    }
    SDL_GL_MakeCurrent(window_, gl_);
    const SDL_Rect s = screen();
    const SDL_Rect pic = picture();
    g_gl.Viewport(0, 0, s.w, s.h);
    g_gl.ClearColor(0.f, 0.f, 0.f, 1.f);
    g_gl.Clear(GL_COLOR_BUFFER_BIT_);
    g_gl.Disable(GL_BLEND_);

    g_gl.UseProgram(gl_program_);
    g_gl.Uniform1i(gl_u_tex_, 0);
    if (gl_u_swizzle_ >= 0) g_gl.Uniform1i(gl_u_swizzle_, 0); // FBO is true RGBA
    g_gl.ActiveTexture(GL_TEXTURE0_);
    g_gl.BindTexture(GL_TEXTURE_2D_, scene_tex);

    const float x0 = 2.f * static_cast<float>(pic.x) / static_cast<float>(s.w) - 1.f;
    const float x1 = 2.f * static_cast<float>(pic.x + pic.w) / static_cast<float>(s.w) - 1.f;
    const float y0 = 1.f - 2.f * static_cast<float>(pic.y + pic.h) / static_cast<float>(s.h);
    const float y1 = 1.f - 2.f * static_cast<float>(pic.y) / static_cast<float>(s.h);
    // Scene FBO: y=0 is NDC top, so the top of the scene sits at high V.
    // Sample with V=1 at the top of the picture (opposite of CPU-upload textures).
    const float scene_verts[] = {
        x0, y0, 0.f, 0.f, x1, y0, 1.f, 0.f, x0, y1, 0.f, 1.f, x1, y1, 1.f, 1.f,
    };
    g_gl.BindBuffer(GL_ARRAY_BUFFER_, gl_vbo_);
    g_gl.BufferData(GL_ARRAY_BUFFER_, sizeof scene_verts, scene_verts, GL_STREAM_DRAW_);
    g_gl.EnableVertexAttribArray(0);
    g_gl.EnableVertexAttribArray(1);
    g_gl.VertexAttribPointer(0, 2, GL_FLOAT_, GL_FALSE_, 16, reinterpret_cast<void*>(0));
    g_gl.VertexAttribPointer(1, 2, GL_FLOAT_, GL_FALSE_, 16, reinterpret_cast<void*>(8));
    g_gl.DrawArrays(GL_TRIANGLE_STRIP_, 0, 4);

    // HUD: upload only the non-transparent bbox (full-buffer TexSubImage was ~20ms on Mali HD).
    if (hud_argb && gl_fb_tex_ && tex_w == fb_w_ && tex_h == fb_h_) {
        int x0b = fb_w_, y0b = fb_h_, x1b = 0, y1b = 0;
        for (int y = 0; y < fb_h_; ++y) {
            const uint32_t* row = hud_argb + static_cast<size_t>(y) * static_cast<size_t>(fb_w_);
            for (int x = 0; x < fb_w_; ++x) {
                if ((row[x] >> 24) == 0) continue;
                if (x < x0b) x0b = x;
                if (x >= x1b) x1b = x + 1;
                if (y < y0b) y0b = y;
                if (y >= y1b) y1b = y + 1;
            }
        }
        if (x1b > x0b && y1b > y0b) {
            // Clear previous HUD so pixels outside the bbox do not ghost.
            if (g_gl.GenFramebuffers && g_gl.BindFramebuffer && g_gl.FramebufferTexture2D) {
                if (!gl_hud_fbo_) {
                    g_gl.GenFramebuffers(1, &gl_hud_fbo_);
                    g_gl.BindFramebuffer(GL_FRAMEBUFFER_, gl_hud_fbo_);
                    g_gl.FramebufferTexture2D(GL_FRAMEBUFFER_, GL_COLOR_ATTACHMENT0_, GL_TEXTURE_2D_, gl_fb_tex_, 0);
                } else {
                    g_gl.BindFramebuffer(GL_FRAMEBUFFER_, gl_hud_fbo_);
                }
                g_gl.Viewport(0, 0, fb_w_, fb_h_);
                g_gl.ClearColor(0.f, 0.f, 0.f, 0.f);
                g_gl.Clear(GL_COLOR_BUFFER_BIT_);
                g_gl.BindFramebuffer(GL_FRAMEBUFFER_, 0);
                g_gl.Viewport(0, 0, s.w, s.h);
            }
            g_gl.BindTexture(GL_TEXTURE_2D_, gl_fb_tex_);
            const int bw = x1b - x0b, bh = y1b - y0b;
#if KURVEN_GLES
            // GLES2 has no UNPACK_ROW_LENGTH — pack the bbox tightly.
            static thread_local std::vector<uint32_t> pack;
            pack.resize(static_cast<size_t>(bw * bh));
            for (int y = 0; y < bh; ++y) {
                const uint32_t* src = hud_argb + static_cast<size_t>(y0b + y) * static_cast<size_t>(fb_w_) +
                                      static_cast<size_t>(x0b);
                std::copy(src, src + bw, pack.begin() + static_cast<size_t>(y) * static_cast<size_t>(bw));
            }
            g_gl.TexSubImage2D(GL_TEXTURE_2D_, 0, x0b, y0b, bw, bh, GL_RGBA_, GL_UNSIGNED_BYTE_, pack.data());
#else
            for (int y = 0; y < bh; ++y) {
                g_gl.TexSubImage2D(GL_TEXTURE_2D_, 0, x0b, y0b + y, bw, 1, GL_BGRA_, GL_UNSIGNED_BYTE_,
                                   hud_argb + static_cast<size_t>(y0b + y) * static_cast<size_t>(fb_w_) +
                                       static_cast<size_t>(x0b));
            }
#endif
            g_gl.Enable(GL_BLEND_);
            g_gl.BlendFunc(GL_SRC_ALPHA_, GL_ONE_MINUS_SRC_ALPHA_);
            if (gl_u_swizzle_ >= 0) g_gl.Uniform1i(gl_u_swizzle_, 1);
            // Buffer y=0 is the top of the picture. NDC y0=bottom, y1=top of the letterbox.
            const float u0 = static_cast<float>(x0b) / static_cast<float>(fb_w_);
            const float u1 = static_cast<float>(x1b) / static_cast<float>(fb_w_);
            const float vt = static_cast<float>(y0b) / static_cast<float>(fb_h_); // top of bbox
            const float vb = static_cast<float>(y1b) / static_cast<float>(fb_h_); // bottom of bbox
            const float fx0 = x0 + (x1 - x0) * static_cast<float>(x0b) / static_cast<float>(fb_w_);
            const float fx1 = x0 + (x1 - x0) * static_cast<float>(x1b) / static_cast<float>(fb_w_);
            const float fyt = y1 + (y0 - y1) * vt; // top of bbox on screen
            const float fyb = y1 + (y0 - y1) * vb; // bottom of bbox on screen
            const float hud_verts[] = {
                fx0, fyb, u0, vb, fx1, fyb, u1, vb, fx0, fyt, u0, vt, fx1, fyt, u1, vt,
            };
            g_gl.BufferData(GL_ARRAY_BUFFER_, sizeof hud_verts, hud_verts, GL_STREAM_DRAW_);
            g_gl.BindTexture(GL_TEXTURE_2D_, gl_fb_tex_);
            g_gl.DrawArrays(GL_TRIANGLE_STRIP_, 0, 4);
            g_gl.Disable(GL_BLEND_);
        }
    }
    g_gl.Enable(GL_BLEND_);
    g_gl.BlendFunc(GL_SRC_ALPHA_, GL_ONE_MINUS_SRC_ALPHA_);
    if (gl_u_swizzle_ >= 0) g_gl.Uniform1i(gl_u_swizzle_, 1); // overlay bitmaps are ARGB
    for (const Overlay::Item& item : overlay.items()) {
        if (!item.image || item.image->w <= 0 || item.image->h <= 0 || item.image->px.empty()) continue;
        const void* key = item.image->px.data();
        unsigned tex = 0;
        if (auto it = gl_overlay_tex_.find(key); it != gl_overlay_tex_.end()) {
            tex = it->second;
        } else {
            g_gl.GenTextures(1, &tex);
            g_gl.BindTexture(GL_TEXTURE_2D_, tex);
            g_gl.TexParameteri(GL_TEXTURE_2D_, GL_TEXTURE_MIN_FILTER_, GL_NEAREST_);
            g_gl.TexParameteri(GL_TEXTURE_2D_, GL_TEXTURE_MAG_FILTER_, GL_NEAREST_);
            g_gl.TexParameteri(GL_TEXTURE_2D_, GL_TEXTURE_WRAP_S_, GL_CLAMP_TO_EDGE_);
            g_gl.TexParameteri(GL_TEXTURE_2D_, GL_TEXTURE_WRAP_T_, GL_CLAMP_TO_EDGE_);
#if KURVEN_GLES
            g_gl.TexImage2D(GL_TEXTURE_2D_, 0, static_cast<GLint>(GL_RGBA_), item.image->w, item.image->h, 0, GL_RGBA_,
                            GL_UNSIGNED_BYTE_, item.image->px.data());
#else
            g_gl.TexImage2D(GL_TEXTURE_2D_, 0, static_cast<GLint>(GL_RGBA_), item.image->w, item.image->h, 0, GL_BGRA_,
                            GL_UNSIGNED_BYTE_, item.image->px.data());
#endif
            gl_overlay_tex_[key] = tex;
        }
        g_gl.BindTexture(GL_TEXTURE_2D_, tex);
        const float ox0 = 2.f * static_cast<float>(item.x) / static_cast<float>(s.w) - 1.f;
        const float ox1 = 2.f * static_cast<float>(item.x + item.image->w) / static_cast<float>(s.w) - 1.f;
        const float oy0 = 1.f - 2.f * static_cast<float>(item.y + item.image->h) / static_cast<float>(s.h);
        const float oy1 = 1.f - 2.f * static_cast<float>(item.y) / static_cast<float>(s.h);
        const float ov[] = {
            ox0, oy0, 0.f, 1.f, ox1, oy0, 1.f, 1.f, ox0, oy1, 0.f, 0.f, ox1, oy1, 1.f, 0.f,
        };
        g_gl.BufferData(GL_ARRAY_BUFFER_, static_cast<GLsizeiptr>(sizeof ov), ov, GL_STREAM_DRAW_);
        g_gl.DrawArrays(GL_TRIANGLE_STRIP_, 0, 4);
    }
    g_gl.Disable(GL_BLEND_);
    (void)tex_w;
    (void)tex_h;
    SDL_GL_SwapWindow(window_);
}

void Display::present_overlay(const Overlay& overlay) {
    if (!is_gl() || !make_gl_current()) return;
    // Overlay items are screen-resolution bitmaps; for now skip if empty and swap.
    // Full overlay support reuses the FB texture path only when items exist.
    if (!overlay.items().empty()) {
        // Draw overlay via temporary upload of a software composite would need
        // a transparent layer; touch controls are optional — swap scene only.
        (void)overlay;
    }
    SDL_GL_SwapWindow(window_);
}

bool Display::make_gl_current() {
    if (!window_ || !gl_) return false;
    return SDL_GL_MakeCurrent(window_, gl_) == 0;
}

void Display::swap_gl() {
    if (window_) SDL_GL_SwapWindow(window_);
}

int Display::window_pixel_width() const {
    int w = 0, h = 0;
    if (window_) SDL_GL_GetDrawableSize(window_, &w, &h);
    return w;
}

int Display::window_pixel_height() const {
    int w = 0, h = 0;
    if (window_) SDL_GL_GetDrawableSize(window_, &w, &h);
    return h;
}

bool save_bmp(const std::string& path, const uint32_t* argb_pixels, int width, int height) {
    if (!argb_pixels || width <= 0 || height <= 0) return false;
    SDL_Surface* surface =
        SDL_CreateRGBSurfaceFrom(const_cast<uint32_t*>(argb_pixels), width, height, 32, width * 4,
                                 0x00ff0000u, 0x0000ff00u, 0x000000ffu, 0xff000000u);
    if (!surface) return false;
    const int rc = SDL_SaveBMP(surface, path.c_str());
    SDL_FreeSurface(surface);
    return rc == 0;
}

} // namespace racer
