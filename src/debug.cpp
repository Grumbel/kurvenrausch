// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "debug.hpp"

#include "people.hpp"
#include "vehicles.hpp"

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

bool DebugMenu::update(const MenuInput& in, DebugOptions& debug, float& hour, int& car, int& driver,
                       int& passenger) {
    if (in.back) return true;
    if (in.up) selected = wrap(selected - 1, items);
    if (in.down) selected = wrap(selected + 1, items);
    if (selected == Back || selected == Sprites || selected == Attract) return in.confirm;
    if (selected == Hour) {
        if (in.left) step_hour(hour, -1);
        if (in.right || in.confirm) step_hour(hour, 1);
        return false;
    }
    if (selected == Car) {
        if (in.left) car = wrap(car - 1, car_models);
        if (in.right || in.confirm) car = wrap(car + 1, car_models);
        return false;
    }
    if (selected == Driver) {
        if (in.left) driver = wrap(driver - 1, drivers);
        if (in.right || in.confirm) driver = wrap(driver + 1, drivers);
        return false;
    }
    if (selected == Passenger) {
        if (in.left) passenger = wrap(passenger - 1, passengers);
        if (in.right || in.confirm) passenger = wrap(passenger + 1, passengers);
        return false;
    }
    if (in.left || in.right || in.confirm) change_debug(debug, selected, in.left ? -1 : 1);
    return false;
}

bool DebugMenu::choose(int item, int side, DebugOptions& debug, float& hour, int& car, int& driver,
                       int& passenger) {
    if (item < 0 || item >= items) return false;
    selected = item;
    if (item == Back || item == Sprites || item == Attract) return true;
    if (item == Hour) {
        step_hour(hour, side < 0 ? -1 : 1);
        return false;
    }
    if (item == Car) {
        car = wrap(car + (side < 0 ? -1 : 1), car_models);
        return false;
    }
    if (item == Driver) {
        driver = wrap(driver + (side < 0 ? -1 : 1), drivers);
        return false;
    }
    if (item == Passenger) {
        passenger = wrap(passenger + (side < 0 ? -1 : 1), passengers);
        return false;
    }
    change_debug(debug, item, side < 0 ? -1 : 1);
    return false;
}

std::string DebugMenu::line(int item, const DebugOptions& d, float hour, int car, int driver_i, int passenger_i) {
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
        case Car: return value("CAR", car_model(car).name);
        case Driver: return value("DRIVER", driver(driver_i).name);
        case Passenger: return value("PASSENGER", passenger(passenger_i).name);
        case Sprites: return "SPRITES…";
        case Attract: return "ATTRACT MODE";
        default: return "BACK";
    }
}

} // namespace racer
