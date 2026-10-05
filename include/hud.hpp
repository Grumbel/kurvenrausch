// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include "framebuffer.hpp"
#include "menu.hpp"
#include "options.hpp"
#include "debug.hpp"
#include "track.hpp"

#include <array>
#include <string>

namespace racer {

struct HudState {
    float speed_fraction = 0.f;  // 0..1 of the car's top speed: the revs
    float speed_kmh_fraction = 0.f; // of the standard car's top speed: the speedometer
    bool reverse = false;        // in reverse gear
    bool signal_left = false;    // the indicator lamps, lit in this moment of their blinking
    bool signal_right = false;
    bool headlights = false;     // the headlights are on
    int lap = 0;                 // 0 before the start line is crossed
    float lap_time = 0.f;
    std::string taxi;            // driving the taxi: where the fare goes, or the fares paid
    std::string time_of_day;     // "HH:MM", empty for none
    float last_lap = 0.f;        // 0 = none yet
    float best_lap = 0.f;
    std::string message;         // centred banner, empty for none
    bool message_visible = false;// for blinking
    bool muted = false;          // sound off
    bool chase = false;          // the police are after the car: show how near it is to escaping
    float escape = 0.f;          // ... 0 .. 1
    bool chase_red = false;      // ... the lightbar's colour now, red or blue
    int nitro = 0;               // canisters left ...
    int nitro_capacity = 3;      // ... of this many (none: no nitro shown)
    float fuel = 1.f;            // 0..1
    bool fuel_warning = false;   // low fuel, blinking
    // Mini map: the plan view from track_map(), the segment the player's car
    // is on, the start line and the gas stations (segments).
    const std::vector<MapPoint>* map = nullptr;
    int map_player = 0;
    int map_start = 0;
    const std::array<std::vector<int>, lot_kinds>* map_lots = nullptr; // each kind's, see Track::lots()
    // Standing at a lot that offers a choice (a car dealer): what it is,
    // what is on offer and, for a car, its top speed, acceleration and grip
    // (factors of the standard car).
    std::string offer_title;     // empty for none
    std::string offer_name;
    bool offer_stats = false;
    float offer_values[3] = {1.f, 1.f, 1.f};
    bool map_blink = false;      // the player's dot blinks
    float map_zoom = 1.f;        // 1 the whole lap; more zooms in around the car
    std::string fork_left;       // a fork ahead: the left route's name
    std::string fork_right;      // ... and the right one's
    float nitro_burn = 0.f;      // fraction left of the canister burning now, 0 for none
    bool attract = false;        // the attract mode: the title and a prompt instead of the dashboard
    bool attract_prompt = false; // the prompt, in this moment of its blinking
    std::string banner;          // country, shown when entering a new zone
    std::string banner_sub;      // region below it
};

void draw_hud(Framebuffer& fb, const HudState& hud);

// The pause menu over the dimmed game; `country` is the start choice's name.
// `place` names where START IN would start, `track` the track picked.
void draw_pause_menu(Framebuffer& fb, const PauseMenu& menu, const std::string& place, const std::string& track);
// The pause menu's OPTIONS page, laid out like the pause menu.
void draw_options_menu(Framebuffer& fb, const OptionsMenu& menu, const Options& options);
void draw_debug_menu(Framebuffer& fb, const DebugMenu& menu, const DebugOptions& debug);

// Formats seconds as m'ss"cc, the classic arcade lap time.
std::string format_lap_time(float seconds);

} // namespace racer
