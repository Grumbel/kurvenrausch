// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "menu.hpp"

namespace racer {

void PauseMenu::open(int current_zone, int zone_count) {
    selected = Resume;
    zones = zone_count > 0 ? zone_count : 1;
    zone = current_zone >= 0 ? current_zone % zones : 0;
}

MenuAction PauseMenu::update(const MenuInput& in) {
    if (in.back) return MenuAction::Resume;
    if (in.up) selected = (selected + items - 1) % items;
    if (in.down) selected = (selected + 1) % items;
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
