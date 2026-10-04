// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "menu.hpp"
#include "overlay.hpp"

#include <cstdint>
#include <map>
#include <vector>

namespace racer {

// A finger on the screen, in screen pixels.
struct Finger {
    int64_t id;
    float x, y;
};

// What the fingers press this frame.
struct TouchInput {
    float steer = 0.f;     // -1 .. +1
    float throttle = 0.f;  // 0 or 1
    float brake = 0.f;
    bool nitro = false;
    bool horn = false;
    bool handbrake = false;
    bool pause = false;    // the pause button went down this frame
};

// Where the touch controls go: the screen's size, and where on it the
// game's picture is, all in screen pixels.
struct TouchLayout {
    float screen_w = 320.f, screen_h = 240.f;
    float game_x = 0.f, game_y = 0.f, game_w = 320.f, game_h = 240.f;
};

// On-screen controls for phones and tablets, laid out over the whole screen
// at its own resolution, black bars beside the picture included: on the left
// a steering pad, steering by how far the finger has moved sideways from
// where it came down; on the right the pedals and buttons; at the top of the
// picture, right of the mirror, a pause button. Sizes are those on a 320x240
// screen, scaled with the screen's height.
class TouchControls {
public:
    enum Button { Gas, Brake, Nitro, Handbrake, Horn, Pause, buttons };
    struct Circle {
        float x, y, r;
    };

    void set_layout(const TouchLayout& layout) { layout_ = layout; }
    const TouchLayout& layout() const { return layout_; }
    // Screen pixels per pixel of a 320x240 screen.
    float unit() const;
    Circle circle(Button b) const;
    float steer_zone_right() const { return 150.f * unit(); } // fingers left of this steer ...
    float steer_zone_top() const { return 40.f * unit(); }    // ... below this
    float steer_travel() const { return 32.f * unit(); }      // sideways for full lock

    // The fingers down now. Keeps track of where each steering finger came
    // down and which buttons are held, for the next frame and for draw().
    TouchInput update(const std::vector<Finger>& fingers);
    // Forget all fingers (when the game pauses, say).
    void release();

    void draw(Overlay& overlay) const;

private:
    TouchLayout layout_;
    std::map<int64_t, float> steer_origin_; // steering fingers: where they came down
    std::map<int64_t, Finger> steering_;    // ... and where they are now
    bool held_[buttons] = {};
    float steer_ = 0.f;
};

// Where a tap on the pause menu landed: the line (-1 for none) and, on the
// country line, its outer thirds: -1 the previous country, +1 the next.
struct MenuTap {
    int item = -1;
    int side = 0;
};
MenuTap menu_tap(const PauseMenu& menu, float x, float y, int fb_width, int fb_height);

} // namespace racer
