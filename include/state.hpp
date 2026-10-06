// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "bindings.hpp"
#include "options.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace racer {

// Where the game keeps what it remembers between runs.
// Prefer $KURVENRAUSCH_STATE_DIR when set (PortMaster: …/ports/kurvenrausch/conf).
// Else XDG: $XDG_STATE_HOME/kurvenrausch, except when XDG_STATE_HOME already ends
// in conf/ (PortMaster sets that to the port conf directory). Else, when the
// binary lives under …/ports/… (or app_base/conf already exists), use
// app_base/conf so a direct ./kurvenrausch still keeps state with the port.
// Else $HOME/.local/state/kurvenrausch. Empty when no absolute path is available.
// app_base is the directory containing the executable (SDL_GetBasePath), or null.
std::string state_dir(const char* xdg_state_home, const char* home, const char* app_base = nullptr);

// What the player chose last time: indices of car_model(), driver() and
// passenger(), the camera view and the radio.
struct Choices {
    int car = 0;
    int driver = 0;
    int passenger = 0;
    int view = 0; // a ViewMode
    int music = 0; // a Music track, -1 off
    int wide = 0;  // 1: the picture as wide as the screen, 0: 4:3
    int track = 0; // see track_name()
    Options options{};
    // Where the last run left off, to go on from there: the position along
    // the track (world units), the time of day (minutes) and the tank (per
    // mille); -1 for none.
    int position = -1;
    int minutes = -1;
    int tank = -1;
    int engine_vol = 10; // 0 .. max_volume
    int music_vol = 10;
    int hd = 0;          // 1: HD framebuffer (pixel_scale 2), 0: SD
    int fullscreen = 0;  // 1: start in fullscreen
    int muted = 0;       // 1: sound off
    int present = 0;     // PresentBackend: 0 Auto, 1 SDL, 2 GL
    int scene = 0;       // SceneBackend: 0 Auto, 1 Software, 2 Gles
    // Debug / VIDEO toggles (were reset every run).
    int dbg_hud = 1;
    int dbg_mirror = 1;
    int dbg_map = 1;
    int dbg_headlights = 1;
    int dbg_weather = 1; // weather overlay
    int dbg_fps = 0;
    int map_zoomed = 1;  // the mini map shows the stretch around the car
    int rumble = 1;      // pad vibration
    int demo_text = 1;   // the attract mode shows the title and the prompt
    int demo_idle = 2;   // see demo_idle_seconds()
    // Keys and pad inputs, as "key_<action> a b" / "pad_<action> a b".
    Bindings bindings = default_bindings();
};
// As "key value" lines (bindings: "key a b"); parsing skips unknown keys and
// malformed lines and keeps the defaults for what is missing.
std::string format_choices(const Choices& c);
Choices parse_choices(std::string_view text);

// One completed lap: when (UTC, ISO 8601), how long, who drove what, and
// where. Lines from before there were tracks to choose are on the first.
struct LapRecord {
    std::string when;
    float seconds = 0.f;
    std::string car;
    std::string driver;
    std::string passenger;
    std::string track = "SMALL WORLD";
};
// A tab-separated line, without the newline; nullopt for a malformed one.
std::string format_lap(const LapRecord& lap);
std::optional<LapRecord> parse_lap(std::string_view line);
// The time now, as LapRecord::when has it.
std::string utc_timestamp();

// The files in the state directory: `choices`, rewritten whole (through a
// temporary file and a rename, so a crash never leaves half of it), and
// `laps.tsv`, every lap ever completed, one per line, appended. Missing
// directories are made with mode 0700. With an empty directory nothing is
// read or written. Failures are reported on stderr, once, and otherwise
// ignored: the game goes on without.
class Store {
public:
    Store() = default;
    explicit Store(std::string dir) : dir_(std::move(dir)) {}

    const std::string& dir() const { return dir_; }
    std::optional<Choices> load_choices() const;
    void save_choices(const Choices& c) const;
    // Print path, env, and a write probe to stderr (startup diagnostics).
    void diagnose() const;
    std::vector<LapRecord> load_laps() const;
    void add_lap(const LapRecord& lap) const;

private:
    bool make_dir() const;
    void fail(const std::string& what) const;

    std::string dir_;
    mutable bool failed_ = false;
};

// The fastest of the laps, 0 for none.
float best_lap(const std::vector<LapRecord>& laps, const std::string& track);

} // namespace racer
