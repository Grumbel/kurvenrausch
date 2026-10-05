// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "display.hpp"

#include "overlay.hpp"

#include <algorithm>
#include <cstring>
#include <iostream>
#include <vector>

#if defined(__EMSCRIPTEN__) || defined(__ANDROID__)
#include <SDL2/SDL_opengles2.h>
#define KURVEN_GLES 1
#else
#include <SDL2/SDL_opengl.h>
#define KURVEN_GLES 0
#endif

namespace racer {

namespace {

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
    "void main(){ gl_FragColor = texture2D(u_tex, v_uv); }\n";
#else
    "#version 110\n"
    "varying vec2 v_uv;\n"
    "uniform sampler2D u_tex;\n"
    "void main(){ gl_FragColor = texture2D(u_tex, v_uv); }\n";
#endif

unsigned compile_shader(unsigned type, const char* src) {
    const unsigned s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    int ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[256];
        glGetShaderInfoLog(s, sizeof log, nullptr, log);
        std::cerr << "kurvenrausch: shader: " << log << "\n";
        glDeleteShader(s);
        return 0;
    }
    return s;
}

unsigned link_program(unsigned vs, unsigned fs) {
    const unsigned p = glCreateProgram();
    glAttachShader(p, vs);
    glAttachShader(p, fs);
    glBindAttribLocation(p, 0, "a_pos");
    glBindAttribLocation(p, 1, "a_uv");
    glLinkProgram(p);
    int ok = 0;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[256];
        glGetProgramInfoLog(p, sizeof log, nullptr, log);
        std::cerr << "kurvenrausch: program: " << log << "\n";
        glDeleteProgram(p);
        return 0;
    }
    return p;
}

} // namespace

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
    if (window_) SDL_DestroyWindow(window_);
    window_ = nullptr;
    if (SDL_WasInit(SDL_INIT_VIDEO)) SDL_QuitSubSystem(SDL_INIT_VIDEO);
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
    if (gl_fb_tex_) {
        glDeleteTextures(1, &gl_fb_tex_);
        gl_fb_tex_ = 0;
    }
    if (gl_vbo_) {
        glDeleteBuffers(1, &gl_vbo_);
        gl_vbo_ = 0;
    }
    if (gl_program_) {
        glDeleteProgram(gl_program_);
        gl_program_ = 0;
    }
    if (gl_) {
        SDL_GL_DeleteContext(gl_);
        gl_ = nullptr;
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
    SDL_GL_SetSwapInterval(1);
    vsync_ = true;

    const unsigned vs = compile_shader(GL_VERTEX_SHADER, k_vert);
    const unsigned fs = compile_shader(GL_FRAGMENT_SHADER, k_frag);
    if (!vs || !fs) {
        if (vs) glDeleteShader(vs);
        if (fs) glDeleteShader(fs);
        return false;
    }
    gl_program_ = link_program(vs, fs);
    glDeleteShader(vs);
    glDeleteShader(fs);
    if (!gl_program_) return false;
    gl_u_tex_ = glGetUniformLocation(gl_program_, "u_tex");

    glGenTextures(1, &gl_fb_tex_);
    glBindTexture(GL_TEXTURE_2D, gl_fb_tex_);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    // Allocate storage; upload each frame with TexSubImage.
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, fb_w_, fb_h_, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    glGenBuffers(1, &gl_vbo_);
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
    glViewport(0, 0, s.w, s.h);
    glClearColor(0.f, 0.f, 0.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT);

    // ARGB8888 in memory is B,G,R,A on little-endian; upload as BGRA when available.
    glBindTexture(GL_TEXTURE_2D, gl_fb_tex_);
#if KURVEN_GLES
    // GLES2 has no BGRA without extension; swizzle by converting to RGBA.
    static thread_local std::vector<uint32_t> rgba;
    rgba.resize(static_cast<size_t>(fb_w_ * fb_h_));
    for (int i = 0; i < fb_w_ * fb_h_; ++i) {
        const uint32_t p = argb_pixels[i];
        rgba[static_cast<size_t>(i)] =
            ((p & 0x000000ffu) << 16) | (p & 0x0000ff00u) | ((p & 0x00ff0000u) >> 16) | (p & 0xff000000u);
    }
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, fb_w_, fb_h_, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
#else
#ifdef GL_BGRA
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, fb_w_, fb_h_, GL_BGRA, GL_UNSIGNED_BYTE, argb_pixels);
#else
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, fb_w_, fb_h_, GL_RGBA, GL_UNSIGNED_BYTE, argb_pixels);
#endif
#endif

    glUseProgram(gl_program_);
    glUniform1i(gl_u_tex_, 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gl_fb_tex_);

    // NDC quad for the letterboxed picture. V flips: FB origin top-left.
    const float x0 = 2.f * static_cast<float>(pic.x) / static_cast<float>(s.w) - 1.f;
    const float x1 = 2.f * static_cast<float>(pic.x + pic.w) / static_cast<float>(s.w) - 1.f;
    const float y0 = 1.f - 2.f * static_cast<float>(pic.y + pic.h) / static_cast<float>(s.h);
    const float y1 = 1.f - 2.f * static_cast<float>(pic.y) / static_cast<float>(s.h);
    const float verts[] = {
        x0, y0, 0.f, 1.f, x1, y0, 1.f, 1.f, x0, y1, 0.f, 0.f, x1, y1, 1.f, 0.f,
    };
    glBindBuffer(GL_ARRAY_BUFFER, gl_vbo_);
    glBufferData(GL_ARRAY_BUFFER, sizeof verts, verts, GL_STREAM_DRAW);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void*>(0));
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                          reinterpret_cast<void*>(2 * sizeof(float)));
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    // Overlay: convert each item to a small texture upload when needed, same shader.
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    for (const Overlay::Item& item : overlay.items()) {
        if (!item.image || item.image->w <= 0 || item.image->h <= 0) continue;
        unsigned tex = 0;
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
#if KURVEN_GLES
        std::vector<uint32_t> o(static_cast<size_t>(item.image->w * item.image->h));
        for (size_t i = 0; i < o.size(); ++i) {
            const uint32_t p = item.image->px[i];
            o[i] = ((p & 0x000000ffu) << 16) | (p & 0x0000ff00u) | ((p & 0x00ff0000u) >> 16) | (p & 0xff000000u);
        }
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, item.image->w, item.image->h, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                     o.data());
#else
#ifdef GL_BGRA
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, item.image->w, item.image->h, 0, GL_BGRA, GL_UNSIGNED_BYTE,
                     item.image->px.data());
#else
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, item.image->w, item.image->h, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                     item.image->px.data());
#endif
#endif
        const float ox0 = 2.f * static_cast<float>(item.x) / static_cast<float>(s.w) - 1.f;
        const float ox1 = 2.f * static_cast<float>(item.x + item.image->w) / static_cast<float>(s.w) - 1.f;
        const float oy0 = 1.f - 2.f * static_cast<float>(item.y + item.image->h) / static_cast<float>(s.h);
        const float oy1 = 1.f - 2.f * static_cast<float>(item.y) / static_cast<float>(s.h);
        const float ov[] = {
            ox0, oy0, 0.f, 1.f, ox1, oy0, 1.f, 1.f, ox0, oy1, 0.f, 0.f, ox1, oy1, 1.f, 0.f,
        };
        glBufferData(GL_ARRAY_BUFFER, sizeof ov, ov, GL_STREAM_DRAW);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glDeleteTextures(1, &tex);
    }
    glDisable(GL_BLEND);

    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    SDL_GL_SwapWindow(window_);
    (void)overlay;
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
