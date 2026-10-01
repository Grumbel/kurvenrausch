// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include "framebuffer.hpp"

#include <string>

namespace racer {

struct HudState {
    float speed_fraction = 0.f;  // 0..1 of top speed
    int lap = 0;                 // 0 before the start line is crossed
    float lap_time = 0.f;
    float last_lap = 0.f;        // 0 = none yet
    float best_lap = 0.f;
    std::string message;         // centred banner, empty for none
    bool message_visible = false;// for blinking
    bool muted = false;          // sound off
    int nitro = 0;               // canisters left
    float nitro_burn = 0.f;      // fraction left of the canister burning now, 0 for none
    std::string banner;          // country, shown when entering a new zone
    std::string banner_sub;      // region below it
};

void draw_hud(Framebuffer& fb, const HudState& hud);

// Formats seconds as m'ss"cc, the classic arcade lap time.
std::string format_lap_time(float seconds);

} // namespace racer
