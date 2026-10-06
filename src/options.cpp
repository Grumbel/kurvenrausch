// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "options.hpp"

#include <algorithm>

namespace racer {

namespace {

int wrap(int v, int n) { return ((v % n) + n) % n; }

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
        case WeatherSetting::Foggy: return 0.f; // front clear; fog applied in look_at
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

const char* time_name(TimeSetting time) {
    static const char* const names[] = {"CYCLE", "DAY", "DUSK", "NIGHT"};
    return names[wrap(static_cast<int>(time), static_cast<int>(TimeSetting::count))];
}

const char* weather_name(WeatherSetting weather) {
    static const char* const names[] = {"CHANGING", "CLEAR", "STORMY", "FOGGY"};
    return names[wrap(static_cast<int>(weather), static_cast<int>(WeatherSetting::count))];
}

const char* traffic_name(int level) {
    static const char* const names[traffic_levels] = {"NONE", "LIGHT", "NORMAL", "HEAVY"};
    return names[std::clamp(level, 0, traffic_levels - 1)];
}

TimeSetting step_setting(TimeSetting time, int dir) {
    return static_cast<TimeSetting>(wrap(static_cast<int>(time) + dir, static_cast<int>(TimeSetting::count)));
}

WeatherSetting step_setting(WeatherSetting weather, int dir) {
    return static_cast<WeatherSetting>(wrap(static_cast<int>(weather) + dir, static_cast<int>(WeatherSetting::count)));
}

float demo_idle_seconds(int choice) {
    static constexpr float seconds[demo_idle_choices] = {0.f, 60.f, 120.f, 300.f};
    return seconds[std::clamp(choice, 0, demo_idle_choices - 1)];
}

const char* demo_idle_name(int choice) {
    static const char* const names[demo_idle_choices] = {"NEVER", "1 MIN", "2 MIN", "5 MIN"};
    return names[std::clamp(choice, 0, demo_idle_choices - 1)];
}

} // namespace racer
