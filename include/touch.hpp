// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "framebuffer.hpp"
#include "menu.hpp"

#include <cstdint>
#include <map>
#include <vector>

namespace racer {

// A finger on the screen, in framebuffer pixels.
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

// On-screen controls for phones and tablets, laid out over the 320x240
// picture: on the left a steering pad, steering by how far the finger has
// moved sideways from where it came down; on the right the pedals and
// buttons; at the top, right of the mirror, a pause button.
class TouchControls {
public:
    enum Button { Gas, Brake, Nitro, Handbrake, Horn, Pause, buttons };
    struct Circle {
        float x, y, r;
    };
    static Circle circle(Button b);
    static constexpr float steer_zone_right = 150.f; // fingers left of this steer ...
    static constexpr float steer_zone_top = 40.f;    // ... below this
    static constexpr float steer_travel = 32.f;      // pixels sideways for full lock

    // The fingers down now. Keeps track of where each steering finger came
    // down and which buttons are held, for the next frame and for draw().
    TouchInput update(const std::vector<Finger>& fingers);
    // Forget all fingers (when the game pauses, say).
    void release();

    void draw(Framebuffer& fb) const;

private:
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
