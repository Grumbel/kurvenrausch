// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include <SDL2/SDL.h>
#include <cstdint>
#include <functional>
#include <map>
#include <string>

namespace racer {

class Overlay;

// How the software framebuffer is shown on the screen. The game still draws
// into a CPU buffer either way; only the final scale-and-blit differs.
enum class PresentBackend {
    Auto = 0, // prefer GL when it starts, else SDL
    Sdl = 1,  // SDL_Renderer + texture upload
    Gl = 2,   // OpenGL ES 2 / desktop GL textured quad
};

const char* present_backend_name(PresentBackend b);
// Cycle AUTO → SDL → GL → AUTO.
PresentBackend next_present_backend(PresentBackend b);

// Who draws the road world (independent of how the framebuffer is presented).
enum class SceneBackend {
    Auto = 0,      // GLES when a GL present path is available, else software
    Software = 1,
    Gles = 2,
};

const char* scene_backend_name(SceneBackend b);
SceneBackend next_scene_backend(SceneBackend b);
bool parse_scene_backend(const char* s, SceneBackend& out);

// Presents a software ARGB8888 framebuffer in an SDL window. The framebuffer
// is scaled to the window with nearest-neighbour filtering and letterboxed to
// keep its aspect ratio; an overlay goes on top at the screen's own
// resolution, across the black bars too.
class Display {
public:
    Display() = default;
    ~Display();
    Display(const Display&) = delete;
    Display& operator=(const Display&) = delete;

    // `app_id` names the application to the desktop (X11 window class,
    // Wayland app ID), so it can be matched to its desktop entry.
    // Opens a window of the framebuffer's size times window_scale, or
    // covering the screen; the framebuffer is scaled to fit either way.
    // `preferred` selects the present path; Auto tries GL then falls back.
    // `hidden` keeps the window unmapped (headless GLES screenshots).
    bool init(const char* title, const char* app_id, int fb_width, int fb_height, int window_scale,
              bool fullscreen = false, PresentBackend preferred = PresentBackend::Auto, bool hidden = false);
    // The window's icon, an ARGB8888 image.
    void set_icon(const uint32_t* argb_pixels, int width, int height);
    void present(const uint32_t* argb_pixels, const Overlay& overlay);
    // Scene texture (RGBA, nearest) letterboxed; then draw_over with the
    // viewport on the picture (framebuffer pixels map onto it), the overlay,
    // and swap. Used by the GLES road path.
    void present_gles_scene(unsigned scene_tex, const std::function<void()>& draw_over, const Overlay& overlay);
    // Scene already drawn into the GL backbuffer; only overlay + swap.
    void present_overlay(const Overlay& overlay);
    // A framebuffer of another size from now on.
    bool resize_framebuffer(int fb_width, int fb_height);
    void toggle_fullscreen();
    bool is_fullscreen() const;

    // Switch present path at runtime (rebuilds the renderer / GL context).
    // Returns false and keeps the previous path when the request fails.
    bool set_present_backend(PresentBackend preferred);
    PresentBackend present_backend() const { return backend_; }
    bool is_gl() const { return backend_ == PresentBackend::Gl && gl_ != nullptr; }
    // Make the GL context current for scene rendering (GLES road path).
    bool make_gl_current();
    void swap_gl();
    // Draw a letterboxed scene already in the default framebuffer at fb size
    // by treating the backbuffer as the scene (scene rendered at window size).
    // Prefer: render scene into gl_fb_tex via FBO later; for now swap after
    // rendering at window resolution.
    int window_pixel_width() const;
    int window_pixel_height() const;

    // The screen's size in pixels (the window's, in pixels rather than
    // points on high-DPI screens).
    SDL_Rect screen() const;
    // Where on the screen the framebuffer goes.
    SDL_Rect picture() const;
    // A touch event's point (0 .. 1 each way across the window) in screen
    // pixels.
    void touch_to_screen(float tx, float ty, float& x, float& y) const;
    // A point in screen pixels on the framebuffer.
    void screen_to_framebuffer(float sx, float sy, float& x, float& y) const;

    // Size of the texture `present()` expects (pixels). Must match the buffer.
    int fb_width() const { return fb_w_; }
    int fb_height() const { return fb_h_; }

    // True if presentation is synchronised to the display refresh; if not the
    // caller should throttle its frame rate itself.
    bool vsync() const { return vsync_; }

private:
    void destroy_present();
    bool init_sdl_present();
    bool init_gl_present();
    void present_sdl(const uint32_t* argb_pixels, const Overlay& overlay);
    void present_gl(const uint32_t* argb_pixels, const Overlay& overlay);
    bool rebuild_present(PresentBackend preferred);

    int fb_w_ = 0;
    int fb_h_ = 0;
    bool vsync_ = false;
    bool fullscreen_wanted_ = false;
    int window_scale_ = 3;
    std::string title_;
    PresentBackend backend_ = PresentBackend::Sdl;
    PresentBackend preferred_ = PresentBackend::Auto;

    SDL_Window* window_ = nullptr;
    // SDL path
    SDL_Renderer* renderer_ = nullptr;
    SDL_Texture* texture_ = nullptr;
    std::map<std::string, SDL_Texture*> overlay_textures_;
    // GL path
    SDL_GLContext gl_ = nullptr;
    unsigned gl_program_ = 0;
    unsigned gl_fb_tex_ = 0;
    unsigned gl_vbo_ = 0;
    int gl_u_tex_ = -1;
    int gl_u_swizzle_ = -1; // 1: sample ARGB uploaded as RGBA (swap R/B)
    // Cached GL textures for overlay bitmaps (key = px.data()).
    std::map<const void*, unsigned> gl_overlay_tex_;
};

// Writes an ARGB8888 pixel buffer as a BMP file. Works without a window.
bool save_bmp(const std::string& path, const uint32_t* argb_pixels, int width, int height);

} // namespace racer
