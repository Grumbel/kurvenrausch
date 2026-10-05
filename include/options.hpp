// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "menu.hpp"

#include <string>

namespace racer {

// Gameplay features the player can switch, in the pause menu's GAME OPTIONS.
enum class TimeSetting { Cycle, Day, Dusk, Night, count };
enum class WeatherSetting { Changing, Clear, Stormy, count };

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

// What GAME OPTIONS returned: stay on the page, go back, or start / change track.
enum class OptionsAction { None, Back, StartZone, ChangeTrack };

// GAME OPTIONS: settings, start-in country, track, BACK.
struct OptionsMenu {
    enum Item { Time, Fuel, Nitros, Police, Weather, Traffic, StartZone, Track, Back, items };

    int selected = Time;
    int zone = 0;
    int zones = 1;
    int track = 0;
    int tracks = 1;

    void open(int current_zone, int zone_count, int current_track, int track_count);
    OptionsAction update(const MenuInput& in, Options& options);
    OptionsAction choose(int item, int side, Options& options);
    // `place` and `track_name` are the labels for StartZone and Track lines.
    static std::string line(int item, const Options& options, const std::string& place,
                            const std::string& track_name);
};

void change(Options& options, int item, int step);

// Video: picture shape, HD, fullscreen.
struct VideoMenu {
    enum Item { Wide, Hd, Fullscreen, Back, items };

    int selected = Wide;

    void open() { selected = Wide; }
    bool update(const MenuInput& in, bool& wide, bool& hd, bool& toggle_fullscreen);
    bool choose(int item, int side, bool& wide, bool& hd, bool& toggle_fullscreen);
    static std::string line(int item, bool wide, bool hd);
};

// Audio: mute and radio track.
struct AudioMenu {
    enum Item { Mute, Radio, Back, items };

    int selected = Mute;

    void open() { selected = Mute; }
    bool update(const MenuInput& in, bool& muted, int& music);
    bool choose(int item, int side, bool& muted, int& music);
    static std::string line(int item, bool muted, int music);
};

} // namespace racer
