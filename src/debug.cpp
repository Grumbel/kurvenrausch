// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "debug.hpp"

namespace racer {

namespace {

int wrap(int v, int n) { return ((v % n) + n) % n; }

const char* on_off(bool v) { return v ? "ON" : "OFF"; }

} // namespace

void change_debug(DebugOptions& d, int item, int /*step*/) {
    switch (item) {
        case DebugMenu::Hud: d.hud = !d.hud; break;
        case DebugMenu::Mirror: d.mirror = !d.mirror; break;
        case DebugMenu::Map: d.map = !d.map; break;
        case DebugMenu::Weather: d.weather = !d.weather; break;
        case DebugMenu::Headlights: d.headlights = !d.headlights; break;
        case DebugMenu::Fps: d.fps = !d.fps; break;
        default: break;
    }
}

bool DebugMenu::update(const MenuInput& in, DebugOptions& debug) {
    if (in.back) return true;
    if (in.up) selected = wrap(selected - 1, items);
    if (in.down) selected = wrap(selected + 1, items);
    if (selected == Back || selected == Sprites || selected == Attract) return in.confirm;
    if (in.left || in.right || in.confirm) change_debug(debug, selected, in.left ? -1 : 1);
    return false;
}

bool DebugMenu::choose(int item, int side, DebugOptions& debug) {
    if (item < 0 || item >= items) return false;
    selected = item;
    if (item == Back || item == Sprites || item == Attract) return true;
    change_debug(debug, item, side < 0 ? -1 : 1);
    return false;
}

std::string DebugMenu::line(int item, const DebugOptions& d) {
    const auto value = [](const std::string& label, const std::string& v) { return label + ": " + v; };
    switch (item) {
        case Hud: return value("HUD", on_off(d.hud));
        case Mirror: return value("MIRROR", on_off(d.mirror));
        case Map: return value("MAP", on_off(d.map));
        case Weather: return value("WEATHER FX", on_off(d.weather));
        case Headlights: return value("HEADLIGHTS", on_off(d.headlights));
        case Fps: return value("FPS", on_off(d.fps));
        case Sprites: return "SPRITES…";
        case Attract: return "ATTRACT MODE";
        default: return "BACK";
    }
}

} // namespace racer
