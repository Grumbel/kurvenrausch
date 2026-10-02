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

enum class MenuAction { None, Resume, Restart, StartZone, Quit };

// The pause menu: resume, restart at the start line, start in a chosen
// country (picked with left and right on its line) or quit.
struct PauseMenu {
    enum Item { Resume, Restart, StartZone, Quit, items };

    int selected = Resume;
    int zone = 0;  // the country to start in
    int zones = 1; // how many there are to choose from

    // Opens the menu on "Resume", with the country the car is in as the
    // start choice.
    void open(int current_zone, int zone_count);
    // Moves the selection and returns what the player chose, if anything.
    MenuAction update(const MenuInput& in);
};

} // namespace racer
