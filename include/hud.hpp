// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include "framebuffer.hpp"
#include "track.hpp"

#include <string>

namespace racer {

struct HudState {
    float speed_fraction = 0.f;  // 0..1 of the car's top speed: the revs
    float speed_kmh_fraction = 0.f; // of the standard car's top speed: the speedometer
    bool reverse = false;        // in reverse gear
    int lap = 0;                 // 0 before the start line is crossed
    float lap_time = 0.f;
    float last_lap = 0.f;        // 0 = none yet
    float best_lap = 0.f;
    std::string message;         // centred banner, empty for none
    bool message_visible = false;// for blinking
    bool muted = false;          // sound off
    int nitro = 0;               // canisters left
    float fuel = 1.f;            // 0..1
    bool fuel_warning = false;   // low fuel, blinking
    // Mini map: the plan view from track_map(), the segment the player's car
    // is on, the start line and the gas stations (segments).
    const std::vector<MapPoint>* map = nullptr;
    int map_player = 0;
    int map_start = 0;
    const std::vector<int>* map_stations = nullptr;
    const std::vector<int>* map_dealers = nullptr;
    // At a car dealer: the car on offer and its top speed, acceleration and
    // grip (factors of the standard car).
    const char* dealer_car = nullptr;
    float dealer_stats[3] = {1.f, 1.f, 1.f};
    bool map_blink = false;      // the player's dot blinks
    std::string fork_left;       // a fork ahead: the left route's name
    std::string fork_right;      // ... and the right one's
    float nitro_burn = 0.f;      // fraction left of the canister burning now, 0 for none
    std::string banner;          // country, shown when entering a new zone
    std::string banner_sub;      // region below it
};

void draw_hud(Framebuffer& fb, const HudState& hud);

// Formats seconds as m'ss"cc, the classic arcade lap time.
std::string format_lap_time(float seconds);

} // namespace racer
