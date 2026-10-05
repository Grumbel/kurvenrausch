// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "menu.hpp"

#include <string>

namespace racer {

// Gameplay features the player can switch, in the pause menu's OPTIONS.
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

// The hour a fixed time of day holds, or -1 for the cycle.
float fixed_hour(TimeSetting time);
// What the weather front is held at (see WeatherFront::force), -1 to drift.
float weather_force(WeatherSetting weather);
// How much traffic, of the normal amount.
float traffic_factor(int level);
// Out-of-range values (from an edited file) brought into range.
Options clamped(const Options& o);

// The OPTIONS page: one line per setting, changed with left and right (or
// confirm, which steps forward), and BACK. Taps: a line's left third steps
// back, the rest forward.
struct OptionsMenu {
    enum Item { Time, Fuel, Nitros, Police, Weather, Traffic, Back, items };

    int selected = Time;

    void open() { selected = Time; }
    // Moves the selection or changes the selected setting; true when the
    // page closes (BACK chosen, or the back button).
    bool update(const MenuInput& in, Options& options);
    // A tap on line `item`, on its left (-1) or right (+1) side.
    bool choose(int item, int side, Options& options);
    // The text of line `item`, such as "FUEL: ON".
    static std::string line(int item, const Options& options);
};

// Steps setting `item` by `step` (+1 or -1), wrapping round.
void change(Options& options, int item, int step);


// Video: picture shape and fullscreen. The game owns the bools; the menu
// only flips them.
struct VideoMenu {
    enum Item { Wide, Fullscreen, Back, items };

    int selected = Wide;

    void open() { selected = Wide; }
    // Sets *toggle_fullscreen when the player chooses FULLSCREEN.
    // Returns true when BACK closes the page.
    bool update(const MenuInput& in, bool& wide, bool& toggle_fullscreen);
    bool choose(int item, int side, bool& wide, bool& toggle_fullscreen);
    static std::string line(int item, bool wide);
};

// Audio: mute and radio track.
struct AudioMenu {
    enum Item { Mute, Music, Back, items };

    int selected = Mute;

    void open() { selected = Mute; }
    bool update(const MenuInput& in, bool& muted, int& music);
    bool choose(int item, int side, bool& muted, int& music);
    static std::string line(int item, bool muted, int music);
};

} // namespace racer
