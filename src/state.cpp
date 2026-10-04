// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "state.hpp"

#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

namespace racer {

namespace fs = std::filesystem;

namespace {

// In a web page the state directory is an IndexedDB mount (see
// mk/wasm/shell.html); written files only reach the browser's storage when
// the file system is synced.
void persist() {
#ifdef __EMSCRIPTEN__
    emscripten_run_script("FS.syncfs(false, function(err) { if (err) console.warn('kurvenrausch: saving state failed', err); });");
#endif
}

} // namespace

std::string state_dir(const char* xdg_state_home, const char* home) {
    if (xdg_state_home && xdg_state_home[0] == '/') return (fs::path(xdg_state_home) / "kurvenrausch").string();
    if (home && home[0] == '/') return (fs::path(home) / ".local" / "state" / "kurvenrausch").string();
    return {};
}

std::string format_choices(const Choices& c) {
    std::ostringstream out;
    out << "car " << c.car << "\n"
        << "driver " << c.driver << "\n"
        << "passenger " << c.passenger << "\n"
        << "view " << c.view << "\n"
        << "music " << c.music << "\n"
        << "wide " << c.wide << "\n"
        << "track " << c.track << "\n"
        << "time " << static_cast<int>(c.options.time) << "\n"
        << "fuel " << (c.options.fuel ? 1 : 0) << "\n"
        << "nitros " << c.options.nitros << "\n"
        << "police " << (c.options.police ? 1 : 0) << "\n"
        << "weather " << static_cast<int>(c.options.weather) << "\n"
        << "traffic " << c.options.traffic << "\n"
        << "position " << c.position << "\n"
        << "minutes " << c.minutes << "\n"
        << "tank " << c.tank << "\n";
    return out.str();
}

Choices parse_choices(std::string_view text) {
    Choices c;
    std::istringstream in{std::string(text)};
    std::string line;
    while (std::getline(in, line)) {
        std::istringstream fields(line);
        std::string key;
        int value = 0;
        if (!(fields >> key >> value)) continue;
        if (key == "car") c.car = value;
        else if (key == "driver") c.driver = value;
        else if (key == "passenger") c.passenger = value;
        else if (key == "view") c.view = value;
        else if (key == "music") c.music = value;
        else if (key == "wide") c.wide = value;
        else if (key == "track") c.track = value;
        else if (key == "time") c.options.time = static_cast<TimeSetting>(value);
        else if (key == "fuel") c.options.fuel = value != 0;
        else if (key == "nitros") c.options.nitros = value;
        else if (key == "police") c.options.police = value != 0;
        else if (key == "weather") c.options.weather = static_cast<WeatherSetting>(value);
        else if (key == "traffic") c.options.traffic = value;
        else if (key == "position") c.position = value;
        else if (key == "minutes") c.minutes = value;
        else if (key == "tank") c.tank = value;
    }
    c.options = clamped(c.options);
    return c;
}

std::string format_lap(const LapRecord& lap) {
    char seconds[32];
    std::snprintf(seconds, sizeof seconds, "%.2f", static_cast<double>(lap.seconds));
    return lap.when + "\t" + seconds + "\t" + lap.car + "\t" + lap.driver + "\t" + lap.passenger + "\t" + lap.track;
}

std::optional<LapRecord> parse_lap(std::string_view line) {
    std::vector<std::string> fields;
    size_t start = 0;
    for (;;) {
        const size_t tab = line.find('\t', start);
        fields.emplace_back(line.substr(start, tab == std::string_view::npos ? std::string_view::npos : tab - start));
        if (tab == std::string_view::npos) break;
        start = tab + 1;
    }
    if ((fields.size() != 5 && fields.size() != 6) || fields[0].empty()) return std::nullopt;
    char* end = nullptr;
    const double seconds = std::strtod(fields[1].c_str(), &end);
    if (end == fields[1].c_str() || *end != '\0' || !(seconds > 0.0)) return std::nullopt;
    LapRecord lap{fields[0], static_cast<float>(seconds), fields[2], fields[3], fields[4]};
    if (fields.size() == 6) lap.track = fields[5];
    return lap;
}

std::string utc_timestamp() {
    const std::time_t now = std::time(nullptr);
    char text[32] = "";
    if (const std::tm* t = std::gmtime(&now)) std::strftime(text, sizeof text, "%Y-%m-%dT%H:%M:%SZ", t);
    return text;
}

float best_lap(const std::vector<LapRecord>& laps, const std::string& track) {
    float best = 0.f;
    for (const LapRecord& lap : laps) {
        if (lap.track == track && (best == 0.f || lap.seconds < best)) best = lap.seconds;
    }
    return best;
}

void Store::fail(const std::string& what) const {
    if (failed_) return;
    failed_ = true;
    std::cerr << "Kurvenrausch: " << what << " (" << dir_ << "); records and choices are not kept\n";
}

bool Store::make_dir() const {
    // Each missing directory on the way, mode 0700 as the spec asks. From
    // the root (/ or C:\): a bare drive (C:) is no directory to test.
    std::error_code ec;
    const fs::path dir(dir_);
    fs::path path = dir.root_path();
    for (const fs::path& part : dir.relative_path()) {
        path /= part;
        if (fs::is_directory(path, ec)) continue;
        if (!fs::create_directory(path, ec) || ec) {
            fail("Cannot create the state directory");
            return false;
        }
        fs::permissions(path, fs::perms::owner_all, ec);
    }
    return true;
}

std::optional<Choices> Store::load_choices() const {
    if (dir_.empty()) return std::nullopt;
    std::ifstream in(fs::path(dir_) / "choices");
    if (!in) return std::nullopt;
    std::ostringstream text;
    text << in.rdbuf();
    return parse_choices(text.str());
}

void Store::save_choices(const Choices& c) const {
    if (dir_.empty() || !make_dir()) return;
    const fs::path file = fs::path(dir_) / "choices", temp = fs::path(dir_) / "choices.tmp";
    {
        std::ofstream out(temp, std::ios::trunc);
        out << format_choices(c);
        out.flush();
        if (!out) {
            fail("Cannot write the choices");
            return;
        }
    }
    std::error_code ec;
    fs::rename(temp, file, ec);
    if (ec) fail("Cannot write the choices");
    else persist();
}

std::vector<LapRecord> Store::load_laps() const {
    std::vector<LapRecord> laps;
    if (dir_.empty()) return laps;
    std::ifstream in(fs::path(dir_) / "laps.tsv");
    std::string line;
    while (std::getline(in, line)) {
        if (std::optional<LapRecord> lap = parse_lap(line)) laps.push_back(std::move(*lap));
    }
    return laps;
}

void Store::add_lap(const LapRecord& lap) const {
    if (dir_.empty() || !make_dir()) return;
    std::ofstream out(fs::path(dir_) / "laps.tsv", std::ios::app);
    out << format_lap(lap) << "\n";
    out.flush();
    if (!out) fail("Cannot write the lap times");
    else persist();
}

} // namespace racer
