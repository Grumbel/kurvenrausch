// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

namespace racer {

// One tick of menu navigation: presses, not held keys.
struct MenuInput {
    bool up = false;
    bool down = false;
    bool left = false;
    bool right = false;
    bool confirm = false;
    bool back = false;
};

enum class MenuAction { None, Resume, Restart, StartZone, ChangeTrack, ToggleWide, Options, Video, Audio, Debug, Quit };

// The pause menu: resume, restart at the start line, start in a chosen
// country (picked with left and right on its line), drive another track
// (picked the same way, confirmed to load it), the screen's shape (4:3
// or as wide as the screen; confirm, left or right switch it, and the menu
// stays open), game options, video, audio (see options.hpp), debug toggles
// and the sprite viewer (see debug.hpp), or quit, where there is something to quit to (not in a web page).
struct PauseMenu {
    enum Item { Resume, Restart, StartZone, Track, Screen, Options, Video, Audio, Debug, Quit, items };

    int selected = Resume;
    int zone = 0;  // the country to start in
    int zones = 1; // how many there are to choose from
    bool can_quit = true;
    bool wide = false; // shown on the Screen line
    int track = 0;     // the track picked on the Track line ...
    int tracks = 1;    // ... of this many

    // Opens the menu on "Resume", with the country the car is in as the
    // start choice.
    void open(int current_zone, int zone_count, bool allow_quit = true);
    // The items shown: all of them, or all but Quit.
    int item_count() const { return can_quit ? items : Quit; }
    // Moves the selection and returns what the player chose, if anything.
    MenuAction update(const MenuInput& in);
    // A tap on line `item`: it is chosen at once; on the country and track
    // lines `side` -1 or +1 picks the previous or next one instead.
    MenuAction choose(int item, int side = 0);
};

} // namespace racer
