// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "display.hpp"

#include <iostream>

namespace racer {

Display::~Display() {
    if (texture_) SDL_DestroyTexture(texture_);
    if (renderer_) SDL_DestroyRenderer(renderer_);
    if (window_) SDL_DestroyWindow(window_);
    if (SDL_WasInit(SDL_INIT_VIDEO)) SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

bool Display::init(const char* title, int fb_width, int fb_height, int window_scale) {
    fb_w_ = fb_width;
    fb_h_ = fb_height;

    if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
        std::cerr << "SDL video init failed: " << SDL_GetError() << "\n";
        return false;
    }

    window_ = SDL_CreateWindow(title,
                               SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                               fb_width * window_scale, fb_height * window_scale,
                               SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (!window_) {
        std::cerr << "Window creation failed: " << SDL_GetError() << "\n";
        return false;
    }

    // Chunky pixels: never smooth the upscaled framebuffer.
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");

    renderer_ = SDL_CreateRenderer(window_, -1,
                                   SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer_) {
        std::cerr << "Accelerated renderer unavailable (" << SDL_GetError()
                  << "), falling back to software\n";
        renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_SOFTWARE);
    }
    if (!renderer_) {
        std::cerr << "Renderer creation failed: " << SDL_GetError() << "\n";
        return false;
    }

    SDL_RendererInfo info;
    if (SDL_GetRendererInfo(renderer_, &info) == 0) {
        vsync_ = (info.flags & SDL_RENDERER_PRESENTVSYNC) != 0;
    }

    SDL_RenderSetLogicalSize(renderer_, fb_width, fb_height);

    texture_ = SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_ARGB8888,
                                 SDL_TEXTUREACCESS_STREAMING, fb_width, fb_height);
    if (!texture_) {
        std::cerr << "Texture creation failed: " << SDL_GetError() << "\n";
        return false;
    }
    return true;
}

void Display::present(const uint32_t* argb_pixels) {
    SDL_UpdateTexture(texture_, nullptr, argb_pixels,
                      fb_w_ * static_cast<int>(sizeof(uint32_t)));
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
    SDL_RenderClear(renderer_);
    SDL_RenderCopy(renderer_, texture_, nullptr, nullptr);
    SDL_RenderPresent(renderer_);
}

void Display::toggle_fullscreen() {
    const bool fullscreen = (SDL_GetWindowFlags(window_) & SDL_WINDOW_FULLSCREEN_DESKTOP) != 0;
    SDL_SetWindowFullscreen(window_, fullscreen ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP);
}

bool save_bmp(const std::string& path, const uint32_t* argb_pixels, int width, int height) {
    // SDL only reads from the buffer here, the const_cast is safe.
    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormatFrom(
        const_cast<uint32_t*>(argb_pixels), width, height, 32,
        width * static_cast<int>(sizeof(uint32_t)), SDL_PIXELFORMAT_ARGB8888);
    if (!surface) {
        std::cerr << "Screenshot surface failed: " << SDL_GetError() << "\n";
        return false;
    }
    const bool ok = SDL_SaveBMP(surface, path.c_str()) == 0;
    if (!ok) std::cerr << "Saving " << path << " failed: " << SDL_GetError() << "\n";
    SDL_FreeSurface(surface);
    return ok;
}

} // namespace racer
