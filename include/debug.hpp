// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "menu.hpp"

#include <string>

namespace racer {

// Technical toggles for development: the pause menu's DEBUG page. Not saved
// with the player's game options; they reset each run.
struct DebugOptions {
    bool hud = true;        // lap times, speedo, bars
    bool mirror = true;     // rear-view mirror
    bool map = true;        // mini map (drawn with the HUD)
    bool weather = true;    // rain and snow overlay
    bool headlights = true; // night headlight beam when the switch is on
    bool fps = false;       // frames-per-second counter
};

// The DEBUG page: toggles, sprite viewer, attract mode, BACK.
struct DebugMenu {
    enum Item { Hud, Mirror, Map, Weather, Headlights, Fps, Sprites, Attract, Back, items };

    int selected = Hud;

    void open() { selected = Hud; }
    // true when the page should close (BACK, Sprites, or Attract).
    bool update(const MenuInput& in, DebugOptions& debug);
    bool choose(int item, int side, DebugOptions& debug);
    static std::string line(int item, const DebugOptions& debug);
};

void change_debug(DebugOptions& debug, int item, int step);

} // namespace racer
