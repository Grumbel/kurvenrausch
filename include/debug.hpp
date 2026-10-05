// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "menu.hpp"

#include <string>

namespace racer {

// Technical toggles for development: the pause menu's DEBUG page. Not saved
// with the player's game options; they reset each run. Weather FX and FPS
// live on the VIDEO page (still stored in DebugOptions).
struct DebugOptions {
    bool hud = true;        // lap times, speedo, bars
    bool mirror = true;     // rear-view mirror
    bool map = true;        // mini map (drawn with the HUD)
    bool weather = true;    // rain and snow overlay (VIDEO menu)
    bool headlights = true; // night headlight beam when the switch is on
    bool fps = false;       // frames-per-second counter (VIDEO menu)
};

// The DEBUG page: display toggles, time of day, sprite viewer, attract, BACK.
struct DebugMenu {
    enum Item { Hud, Mirror, Map, Headlights, Hour, Sprites, Attract, Back, items };

    int selected = Hud;

    void open() { selected = Hud; }
    // true when the page should close (BACK, Sprites, or Attract).
    // `hour` is the current time of day (0 .. 24); left/right on Hour steps it.
    bool update(const MenuInput& in, DebugOptions& debug, float& hour);
    bool choose(int item, int side, DebugOptions& debug, float& hour);
    static std::string line(int item, const DebugOptions& debug, float hour);
};

void change_debug(DebugOptions& debug, int item, int step);
// Step the clock by `step` hours (wrapped 0 .. 24).
void step_hour(float& hour, int step);

} // namespace racer
