// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "menu.hpp"

namespace racer {

void PauseMenu::open(bool allow_quit) {
    selected = Resume;
    can_quit = allow_quit;
}

MenuAction PauseMenu::choose(int item, int /*side*/) {
    if (item < 0 || item >= item_count()) return MenuAction::None;
    selected = item;
    MenuInput confirm;
    confirm.confirm = true;
    return update(confirm);
}

MenuAction PauseMenu::update(const MenuInput& in) {
    if (in.back) return MenuAction::Resume;
    const int n = item_count();
    if (in.up) selected = (selected + n - 1) % n;
    if (in.down) selected = (selected + 1) % n;
    if (!in.confirm) return MenuAction::None;
    switch (selected) {
        case Resume: return MenuAction::Resume;
        case Restart: return MenuAction::Restart;
        case Options: return MenuAction::Options;
        case Video: return MenuAction::Video;
        case Audio: return MenuAction::Audio;
        case Debug: return MenuAction::Debug;
        default: return MenuAction::Quit;
    }
}

} // namespace racer
