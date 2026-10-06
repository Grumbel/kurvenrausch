// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

namespace racer {

// What the player can set for the race (RACE SETUP and GAMEPLAY in the
// pause menu), kept in the choices file.
enum class TimeSetting { Cycle, Day, Dusk, Night, count };
enum class WeatherSetting { Changing, Clear, Stormy, Foggy, count };

struct Options {
    TimeSetting time = TimeSetting::Cycle; // the day passing, or a fixed time
    bool fuel = true;                      // off: the tank never empties
    int nitros = 3;                        // canisters each lap, 0 .. max_nitros
    bool police = true;                    // chases now and then
    WeatherSetting weather = WeatherSetting::Changing;
    int traffic = 2;                       // 0 none, 1 light, 2 normal, 3 heavy
};
constexpr int max_nitros = 9;
constexpr int traffic_levels = 4;

float fixed_hour(TimeSetting time);
float weather_force(WeatherSetting weather);
float traffic_factor(int level);
Options clamped(const Options& o);

const char* time_name(TimeSetting time);
const char* weather_name(WeatherSetting weather);
const char* traffic_name(int level);
// One step through an enum's values, wrapping round.
TimeSetting step_setting(TimeSetting time, int dir);
WeatherSetting step_setting(WeatherSetting weather, int dir);

// The attract mode after a while without input: off, 1, 2 or 5 minutes.
constexpr int demo_idle_choices = 4;
float demo_idle_seconds(int choice); // 0 for off
const char* demo_idle_name(int choice);

// Engine / effects and music levels.
constexpr int max_volume = 10;

} // namespace racer
