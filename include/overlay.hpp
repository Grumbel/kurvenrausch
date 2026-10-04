// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include "bitmap.hpp"
#include "framebuffer.hpp"
#include "types.hpp"

#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace racer {

// Pictures laid over the game at the screen's own resolution, such as the
// touch controls: the game's framebuffer is scaled up behind them, they are
// not. Each shape becomes an image with an alpha channel, made once per look
// and kept, placed in screen pixels.
class Overlay {
public:
    struct Item {
        const std::string* key; // names the image: same key, same pixels
        const Bitmap* image;
        int x, y;               // top-left, screen pixels
    };

    // Starts a new frame's shapes; the images are kept.
    void clear() { items_.clear(); }
    // A translucent disc with a white rim `rim` pixels wide, brighter while
    // pressed.
    void disc(float cx, float cy, float r, float rim, bool pressed);
    // Text in the bitmap font at `scale`, with a drop shadow, centred on
    // (cx, cy).
    void text(float cx, float cy, std::string_view s, int scale);
    // A solid rectangle.
    void rect(float x, float y, float w, float h, Color c);

    const std::vector<Item>& items() const { return items_; }
    // Draws the shapes into a framebuffer whose pixels are the screen's (the
    // headless screenshots).
    void draw(Framebuffer& fb) const;

private:
    // Places the image named `key` with its top-left at (x, y), making it
    // with make() the first time.
    template <typename Make>
    void place(std::string key, int x, int y, Make&& make);

    std::map<std::string, Bitmap> images_;
    std::vector<Item> items_;
};

} // namespace racer
