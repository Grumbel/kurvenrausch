// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "options.hpp"
#include "music.hpp"

#include <algorithm>

namespace racer {

namespace {

int wrap(int v, int n) { return ((v % n) + n) % n; }

const char* on_off(bool on) { return on ? "ON" : "OFF"; }

} // namespace

float fixed_hour(TimeSetting time) {
    switch (time) {
        case TimeSetting::Day: return 12.f;
        case TimeSetting::Dusk: return 18.f;
        case TimeSetting::Night: return 23.f;
        default: return -1.f;
    }
}

float weather_force(WeatherSetting weather) {
    switch (weather) {
        case WeatherSetting::Clear: return 0.f;
        case WeatherSetting::Stormy: return 1.f;
        default: return -1.f;
    }
}

float traffic_factor(int level) {
    static constexpr float factors[traffic_levels] = {0.f, 0.5f, 1.f, 2.f};
    return factors[std::clamp(level, 0, traffic_levels - 1)];
}

Options clamped(const Options& o) {
    Options c = o;
    c.time = static_cast<TimeSetting>(wrap(static_cast<int>(o.time), static_cast<int>(TimeSetting::count)));
    c.weather = static_cast<WeatherSetting>(wrap(static_cast<int>(o.weather), static_cast<int>(WeatherSetting::count)));
    c.nitros = std::clamp(o.nitros, 0, max_nitros);
    c.traffic = std::clamp(o.traffic, 0, traffic_levels - 1);
    return c;
}

void change(Options& o, int item, int step) {
    const int s = step < 0 ? -1 : 1;
    switch (item) {
        case OptionsMenu::Time:
            o.time = static_cast<TimeSetting>(wrap(static_cast<int>(o.time) + s, static_cast<int>(TimeSetting::count)));
            break;
        case OptionsMenu::Fuel: o.fuel = !o.fuel; break;
        case OptionsMenu::Nitros: o.nitros = wrap(o.nitros + s, max_nitros + 1); break;
        case OptionsMenu::Police: o.police = !o.police; break;
        case OptionsMenu::Weather:
            o.weather = static_cast<WeatherSetting>(wrap(static_cast<int>(o.weather) + s,
                                                         static_cast<int>(WeatherSetting::count)));
            break;
        case OptionsMenu::Traffic: o.traffic = wrap(o.traffic + s, traffic_levels); break;
        default: break;
    }
}

bool OptionsMenu::update(const MenuInput& in, Options& options) {
    if (in.back) return true;
    if (in.up) selected = wrap(selected - 1, items);
    if (in.down) selected = wrap(selected + 1, items);
    if (selected == Back) return in.confirm;
    if (in.left) change(options, selected, -1);
    if (in.right || in.confirm) change(options, selected, +1);
    return false;
}

bool OptionsMenu::choose(int item, int side, Options& options) {
    if (item < 0 || item >= items) return false;
    selected = item;
    if (item == Back) return true;
    change(options, item, side < 0 ? -1 : 1);
    return false;
}

std::string OptionsMenu::line(int item, const Options& o) {
    static const char* times[] = {"CYCLE", "DAY", "DUSK", "NIGHT"};
    static const char* weathers[] = {"CHANGING", "CLEAR", "STORMY"};
    static const char* traffic[] = {"NONE", "LIGHT", "NORMAL", "HEAVY"};
    const auto value = [](const std::string& label, const std::string& v) { return label + ": " + v; };
    switch (item) {
        case Time: return value("TIME", times[static_cast<int>(o.time)]);
        case Fuel: return value("FUEL", on_off(o.fuel));
        case Nitros: return value("NITRO", std::to_string(o.nitros));
        case Police: return value("POLICE", on_off(o.police));
        case Weather: return value("WEATHER", weathers[static_cast<int>(o.weather)]);
        case Traffic: return value("TRAFFIC", traffic[std::clamp(o.traffic, 0, traffic_levels - 1)]);
        default: return "BACK";
    }
}

} // namespace racer

bool VideoMenu::update(const MenuInput& in, bool& wide, bool& toggle_fullscreen) {
    if (in.back) return true;
    if (in.up) selected = ((selected - 1) % items + items) % items;
    if (in.down) selected = (selected + 1) % items;
    if (selected == Back) return in.confirm;
    if (selected == Wide && (in.left || in.right || in.confirm)) wide = !wide;
    if (selected == Fullscreen && (in.left || in.right || in.confirm)) toggle_fullscreen = true;
    return false;
}

bool VideoMenu::choose(int item, int side, bool& wide, bool& toggle_fullscreen) {
    if (item < 0 || item >= items) return false;
    selected = item;
    if (item == Back) return true;
    if (item == Wide) wide = !wide;
    if (item == Fullscreen) toggle_fullscreen = true;
    return false;
}

std::string VideoMenu::line(int item, bool wide) {
    switch (item) {
        case Wide: return std::string("SCREEN: ") + (wide ? "WIDE" : "4:3");
        case Fullscreen: return "FULLSCREEN";
        default: return "BACK";
    }
}

bool AudioMenu::update(const MenuInput& in, bool& muted, int& music) {
    if (in.back) return true;
    if (in.up) selected = ((selected - 1) % items + items) % items;
    if (in.down) selected = (selected + 1) % items;
    if (selected == Back) return in.confirm;
    if (selected == Mute && (in.left || in.right || in.confirm)) muted = !muted;
    if (selected == Music) {
        if (in.left) music = Music::previous(music);
        if (in.right || in.confirm) music = Music::next(music);
    }
    return false;
}

bool AudioMenu::choose(int item, int side, bool& muted, int& music) {
    if (item < 0 || item >= items) return false;
    selected = item;
    if (item == Back) return true;
    if (item == Mute) muted = !muted;
    if (item == Music) music = side < 0 ? Music::previous(music) : Music::next(music);
    return false;
}

std::string AudioMenu::line(int item, bool muted, int music) {
    switch (item) {
        case Mute: return std::string("MUTE: ") + (muted ? "ON" : "OFF");
        case Music: return std::string("MUSIC: ") + Music::name(music);
        default: return "BACK";
    }
}
