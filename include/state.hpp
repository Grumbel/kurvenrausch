// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace racer {

// Where the game keeps what it remembers between runs, following the XDG Base
// Directory spec: $XDG_STATE_HOME/kurvenrausch, or
// $HOME/.local/state/kurvenrausch when XDG_STATE_HOME is unset, empty or
// relative (the spec says to ignore relative paths). Empty when neither
// gives an absolute path.
std::string state_dir(const char* xdg_state_home, const char* home);

// What the player chose last time: indices of car_model(), driver() and
// passenger(), and the car left at a truck stop.
struct Choices {
    int car = 0;
    int car_before_truck = 0;
    int driver = 0;
    int passenger = 0;
    int view = 0; // a ViewMode
};
// As "key value" lines; parsing skips unknown keys and malformed lines and
// keeps the defaults for what is missing.
std::string format_choices(const Choices& c);
Choices parse_choices(std::string_view text);

// One completed lap: when (UTC, ISO 8601), how long, and who drove what.
struct LapRecord {
    std::string when;
    float seconds = 0.f;
    std::string car;
    std::string driver;
    std::string passenger;
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
    std::vector<LapRecord> load_laps() const;
    void add_lap(const LapRecord& lap) const;

private:
    bool make_dir() const;
    void fail(const std::string& what) const;

    std::string dir_;
    mutable bool failed_ = false;
};

// The fastest of the laps, 0 for none.
float best_lap(const std::vector<LapRecord>& laps);

} // namespace racer
