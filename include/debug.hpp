// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

namespace racer {

// What is drawn over the road (VIDEO in the pause menu) and the headlight
// beam (DEBUG). Persisted in choices (dbg_*).
struct DebugOptions {
    bool hud = true;        // lap times, speedo, bars
    bool mirror = true;     // rear-view mirror
    bool map = true;        // mini map (drawn with the HUD)
    bool weather = true;    // rain and snow overlay (VIDEO menu)
    bool headlights = true; // night headlight beam when the switch is on
    bool fps = false;       // frames-per-second counter (VIDEO menu)
};

// Step the clock by `step` hours (wrapped 0 .. 24).
void step_hour(float& hour, int step);

} // namespace racer
