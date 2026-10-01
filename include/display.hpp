#pragma once
#include <SDL2/SDL.h>
#include <cstdint>
#include <string>

namespace racer {

// Presents a software ARGB8888 framebuffer in an SDL window. The framebuffer
// is scaled to the window with nearest-neighbour filtering and letterboxed to
// keep its aspect ratio.
class Display {
public:
    Display() = default;
    ~Display();
    Display(const Display&) = delete;
    Display& operator=(const Display&) = delete;

    bool init(const char* title, int fb_width, int fb_height, int window_scale);
    void present(const uint32_t* argb_pixels);
    void toggle_fullscreen();

    // True if presentation is synchronised to the display refresh; if not the
    // caller should throttle its frame rate itself.
    bool vsync() const { return vsync_; }

private:
    int fb_w_ = 0;
    int fb_h_ = 0;
    bool vsync_ = false;
    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    SDL_Texture* texture_ = nullptr;
};

// Writes an ARGB8888 pixel buffer as a BMP file. Works without a window.
bool save_bmp(const std::string& path, const uint32_t* argb_pixels, int width, int height);

} // namespace racer
