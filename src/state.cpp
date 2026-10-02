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

namespace racer {

namespace fs = std::filesystem;

std::string state_dir(const char* xdg_state_home, const char* home) {
    if (xdg_state_home && xdg_state_home[0] == '/') return (fs::path(xdg_state_home) / "kurvenrausch").string();
    if (home && home[0] == '/') return (fs::path(home) / ".local" / "state" / "kurvenrausch").string();
    return {};
}

std::string format_choices(const Choices& c) {
    std::ostringstream out;
    out << "car " << c.car << "\n"
        << "car_before_truck " << c.car_before_truck << "\n"
        << "driver " << c.driver << "\n"
        << "passenger " << c.passenger << "\n"
        << "view " << c.view << "\n";
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
        else if (key == "car_before_truck") c.car_before_truck = value;
        else if (key == "driver") c.driver = value;
        else if (key == "passenger") c.passenger = value;
        else if (key == "view") c.view = value;
    }
    return c;
}

std::string format_lap(const LapRecord& lap) {
    char seconds[32];
    std::snprintf(seconds, sizeof seconds, "%.2f", static_cast<double>(lap.seconds));
    return lap.when + "\t" + seconds + "\t" + lap.car + "\t" + lap.driver + "\t" + lap.passenger;
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
    if (fields.size() != 5 || fields[0].empty()) return std::nullopt;
    char* end = nullptr;
    const double seconds = std::strtod(fields[1].c_str(), &end);
    if (end == fields[1].c_str() || *end != '\0' || !(seconds > 0.0)) return std::nullopt;
    return LapRecord{fields[0], static_cast<float>(seconds), fields[2], fields[3], fields[4]};
}

std::string utc_timestamp() {
    const std::time_t now = std::time(nullptr);
    char text[32] = "";
    if (const std::tm* t = std::gmtime(&now)) std::strftime(text, sizeof text, "%Y-%m-%dT%H:%M:%SZ", t);
    return text;
}

float best_lap(const std::vector<LapRecord>& laps) {
    float best = 0.f;
    for (const LapRecord& lap : laps) {
        if (best == 0.f || lap.seconds < best) best = lap.seconds;
    }
    return best;
}

void Store::fail(const std::string& what) const {
    if (failed_) return;
    failed_ = true;
    std::cerr << "Kurvenrausch: " << what << " (" << dir_ << "); records and choices are not kept\n";
}

bool Store::make_dir() const {
    // Each missing directory on the way, mode 0700 as the spec asks.
    std::error_code ec;
    fs::path path;
    for (const fs::path& part : fs::path(dir_)) {
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
}

} // namespace racer
