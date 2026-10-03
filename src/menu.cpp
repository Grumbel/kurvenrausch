// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "menu.hpp"

namespace racer {

void PauseMenu::open(int current_zone, int zone_count, bool allow_quit) {
    selected = Resume;
    can_quit = allow_quit;
    zones = zone_count > 0 ? zone_count : 1;
    zone = current_zone >= 0 ? current_zone % zones : 0;
}

MenuAction PauseMenu::choose(int item, int side) {
    if (item < 0 || item >= item_count()) return MenuAction::None;
    selected = item;
    if (item == StartZone && side != 0) {
        zone = (zone + side + zones) % zones;
        return MenuAction::None;
    }
    MenuInput confirm;
    confirm.confirm = true;
    return update(confirm);
}

MenuAction PauseMenu::update(const MenuInput& in) {
    if (in.back) return MenuAction::Resume;
    const int n = item_count();
    if (in.up) selected = (selected + n - 1) % n;
    if (in.down) selected = (selected + 1) % n;
    if (selected == StartZone) {
        if (in.left) zone = (zone + zones - 1) % zones;
        if (in.right) zone = (zone + 1) % zones;
    }
    if (!in.confirm) return MenuAction::None;
    switch (selected) {
        case Resume: return MenuAction::Resume;
        case Restart: return MenuAction::Restart;
        case StartZone: return MenuAction::StartZone;
        default: return MenuAction::Quit;
    }
}

} // namespace racer
