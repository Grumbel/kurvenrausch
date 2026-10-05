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

enum class MenuAction { None, Resume, Restart, Options, Video, Audio, Debug, Quit };

// The pause menu: resume, restart, game options / video / audio / debug pages,
// or quit (where there is something to quit to). Start-in country, track, and
// screen shape live on those pages (see options.hpp).
struct PauseMenu {
    enum Item { Resume, Restart, Options, Video, Audio, Debug, Quit, items };

    int selected = Resume;
    bool can_quit = true;

    void open(bool allow_quit = true);
    int item_count() const { return can_quit ? items : Quit; }
    MenuAction update(const MenuInput& in);
    MenuAction choose(int item, int side = 0);
};

} // namespace racer
