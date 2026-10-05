// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "debug.hpp"

#include <cmath>
#include <cstdio>

namespace racer {

namespace {

int wrap(int v, int n) { return ((v % n) + n) % n; }

const char* on_off(bool v) { return v ? "ON" : "OFF"; }

} // namespace

void step_hour(float& hour, int step) {
    hour = std::fmod(hour + static_cast<float>(step), 24.f);
    if (hour < 0.f) hour += 24.f;
}

void change_debug(DebugOptions& d, int item, int /*step*/) {
    switch (item) {
        case DebugMenu::Hud: d.hud = !d.hud; break;
        case DebugMenu::Mirror: d.mirror = !d.mirror; break;
        case DebugMenu::Map: d.map = !d.map; break;
        case DebugMenu::Headlights: d.headlights = !d.headlights; break;
        default: break;
    }
}

bool DebugMenu::update(const MenuInput& in, DebugOptions& debug, float& hour) {
    if (in.back) return true;
    if (in.up) selected = wrap(selected - 1, items);
    if (in.down) selected = wrap(selected + 1, items);
    if (selected == Back || selected == Sprites || selected == Attract) return in.confirm;
    if (selected == Hour) {
        if (in.left) step_hour(hour, -1);
        if (in.right || in.confirm) step_hour(hour, 1);
        return false;
    }
    if (in.left || in.right || in.confirm) change_debug(debug, selected, in.left ? -1 : 1);
    return false;
}

bool DebugMenu::choose(int item, int side, DebugOptions& debug, float& hour) {
    if (item < 0 || item >= items) return false;
    selected = item;
    if (item == Back || item == Sprites || item == Attract) return true;
    if (item == Hour) {
        step_hour(hour, side < 0 ? -1 : 1);
        return false;
    }
    change_debug(debug, item, side < 0 ? -1 : 1);
    return false;
}

std::string DebugMenu::line(int item, const DebugOptions& d, float hour) {
    const auto value = [](const std::string& label, const std::string& v) { return label + ": " + v; };
    switch (item) {
        case Hud: return value("HUD", on_off(d.hud));
        case Mirror: return value("MIRROR", on_off(d.mirror));
        case Map: return value("MAP", on_off(d.map));
        case Headlights: return value("HEADLIGHTS", on_off(d.headlights));
        case Hour: {
            char buf[32];
            const int h = static_cast<int>(std::floor(hour)) % 24;
            const int m = static_cast<int>(std::floor(std::fmod(hour, 1.f) * 60.f));
            std::snprintf(buf, sizeof(buf), "%02d:%02d", h, m);
            return value("HOUR", buf);
        }
        case Sprites: return "SPRITES…";
        case Attract: return "ATTRACT MODE";
        default: return "BACK";
    }
}

} // namespace racer
