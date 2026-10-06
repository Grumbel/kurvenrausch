// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "state.hpp"

#include <SDL2/SDL.h>

#include <algorithm>
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

std::string state_dir(const char* xdg_state_home, const char* home, const char* app_base) {
    // PortMaster / handheld: explicit root (e.g. /roms/ports/kurvenrausch/conf).
    if (const char* forced = std::getenv("KURVENRAUSCH_STATE_DIR"); forced && forced[0] == '/')
        return forced;
    if (xdg_state_home && xdg_state_home[0] == '/') {
        const fs::path base(xdg_state_home);
        // PortMaster sets XDG_STATE_HOME to the port's conf/ directory itself —
        // do not nest another kurvenrausch/ under it.
        if (base.filename() == "conf") return base.string();
        return (base / "kurvenrausch").string();
    }
    // Binary next to conf/ under a PortMaster ports/ tree (or conf already there).
    if (app_base && app_base[0] == '/') {
        std::string root = app_base;
        while (!root.empty() && (root.back() == '/' || root.back() == '\\')) root.pop_back();
        const fs::path base(root);
        const fs::path conf = base / "conf";
        std::error_code ec;
        const bool under_ports =
            root.find("/ports/") != std::string::npos || root.find("/Ports/") != std::string::npos;
        if (under_ports || fs::is_directory(conf, ec)) return conf.string();
    }
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
        << "tank " << c.tank << "\n"
        << "engine_vol " << c.engine_vol << "\n"
        << "music_vol " << c.music_vol << "\n"
        << "hd " << c.hd << "\n"
        << "fullscreen " << c.fullscreen << "\n"
        << "muted " << c.muted << "\n"
        << "present " << c.present << "\n"
        << "scene " << c.scene << "\n"
        << "dbg_hud " << c.dbg_hud << "\n"
        << "dbg_mirror " << c.dbg_mirror << "\n"
        << "dbg_map " << c.dbg_map << "\n"
        << "dbg_headlights " << c.dbg_headlights << "\n"
        << "dbg_weather " << c.dbg_weather << "\n"
        << "dbg_fps " << c.dbg_fps << "\n"
        << "map_zoomed " << c.map_zoomed << "\n"
        << "rumble " << c.rumble << "\n"
        << "demo_text " << c.demo_text << "\n"
        << "demo_idle " << c.demo_idle << "\n";
    for (int a = 0; a < action_count; ++a) {
        const auto& keys = c.bindings.keys[static_cast<size_t>(a)];
        const auto& pad = c.bindings.pad[static_cast<size_t>(a)];
        out << "key_" << action_id(static_cast<Action>(a)) << " " << keys[0] << " " << keys[1] << "\n"
            << "pad_" << action_id(static_cast<Action>(a)) << " " << pad[0] << " " << pad[1] << "\n";
    }
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
        else if (key == "engine_vol") c.engine_vol = value;
        else if (key == "music_vol") c.music_vol = value;
        else if (key == "hd") c.hd = value;
        else if (key == "fullscreen") c.fullscreen = value;
        else if (key == "muted") c.muted = value;
        else if (key == "present") c.present = value;
        else if (key == "scene") c.scene = value;
        else if (key == "dbg_hud") c.dbg_hud = value;
        else if (key == "dbg_mirror") c.dbg_mirror = value;
        else if (key == "dbg_map") c.dbg_map = value;
        else if (key == "dbg_headlights") c.dbg_headlights = value;
        else if (key == "dbg_weather") c.dbg_weather = value;
        else if (key == "dbg_fps") c.dbg_fps = value;
        else if (key == "map_zoomed") c.map_zoomed = value;
        else if (key == "rumble") c.rumble = value;
        else if (key == "demo_text") c.demo_text = value;
        else if (key == "demo_idle") c.demo_idle = std::clamp(value, 0, demo_idle_choices - 1);
        else if (key.size() > 4 && (key.compare(0, 4, "key_") == 0 || key.compare(0, 4, "pad_") == 0)) {
            int second = 0;
            if (!(fields >> second)) second = 0;
            const bool is_key = key[0] == 'k';
            const auto valid = [&](int v) {
                return is_key ? v >= 0 && v < SDL_NUM_SCANCODES : v == pad_none || pad_is_button(v) || pad_is_axis(v);
            };
            if (!valid(value) || !valid(second)) continue;
            for (int a = 0; a < action_count; ++a) {
                if (key.compare(4, std::string::npos, action_id(static_cast<Action>(a))) != 0) continue;
                auto& slots = is_key ? c.bindings.keys[static_cast<size_t>(a)] : c.bindings.pad[static_cast<size_t>(a)];
                slots = {value, second};
            }
        }
    }
    c.options = clamped(c.options);
    c.engine_vol = std::clamp(c.engine_vol, 0, max_volume);
    c.music_vol = std::clamp(c.music_vol, 0, max_volume);
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

void Store::diagnose() const {
    std::cerr << "Kurvenrausch: state diagnostics\n";
    const char* forced = std::getenv("KURVENRAUSCH_STATE_DIR");
    const char* xdg = std::getenv("XDG_STATE_HOME");
    const char* home = std::getenv("HOME");
    std::cerr << "  KURVENRAUSCH_STATE_DIR=" << (forced ? forced : "(unset)") << "\n";
    std::cerr << "  XDG_STATE_HOME=" << (xdg ? xdg : "(unset)") << "\n";
    std::cerr << "  HOME=" << (home ? home : "(unset)") << "\n";
    std::cerr << "  resolved dir=" << (dir_.empty() ? "(empty — saves disabled)" : dir_) << "\n";
    if (dir_.empty()) {
        std::cerr << "  result: no state directory; choices will not be saved or loaded\n";
        return;
    }
    std::error_code ec;
    const fs::path dir(dir_);
    const bool exists = fs::exists(dir, ec);
    const bool is_dir = exists && fs::is_directory(dir, ec);
    std::cerr << "  exists=" << (exists ? "yes" : "no")
              << " is_directory=" << (is_dir ? "yes" : "no");
    if (ec) std::cerr << " (" << ec.message() << ")";
    std::cerr << "\n";
    if (!is_dir) {
        std::cerr << "  creating directory...\n";
        if (!make_dir()) {
            std::cerr << "  result: cannot create directory; saves disabled\n";
            return;
        }
        std::cerr << "  created OK\n";
    }
    const fs::path probe = dir / "choices.write-probe";
    const fs::path target = dir / "choices";
    {
        std::ofstream out(probe, std::ios::trunc);
        out << "probe\n";
        out.flush();
        if (!out) {
            std::cerr << "  write probe: FAILED to open/write " << probe << "\n";
            std::cerr << "  result: directory not writable; saves will fail\n";
            return;
        }
    }
    std::cerr << "  write probe: wrote " << probe << "\n";
    fs::remove(probe, ec);
    if (ec) std::cerr << "  remove probe: " << ec.message() << "\n";
    else std::cerr << "  remove probe: OK\n";
    // Same sequence as save_choices: write temp, rename over choices if present.
    const fs::path temp = dir / "choices.tmp";
    {
        std::ofstream out(temp, std::ios::trunc);
        out << "probe-rename\n";
        out.flush();
        if (!out) {
            std::cerr << "  rename probe: FAILED to write " << temp << "\n";
            return;
        }
    }
    fs::rename(temp, dir / "choices.rename-probe", ec);
    if (ec) {
        std::cerr << "  rename probe: FAILED " << temp << " -> choices.rename-probe: " << ec.message()
                  << "\n";
        fs::remove(temp, ec);
    } else {
        std::cerr << "  rename probe: OK\n";
        fs::remove(dir / "choices.rename-probe", ec);
    }
    if (fs::exists(target, ec)) {
        const auto sz = fs::file_size(target, ec);
        std::cerr << "  existing choices: yes";
        if (!ec) std::cerr << " size=" << sz << " bytes";
        std::cerr << "\n";
    } else {
        std::cerr << "  existing choices: no\n";
    }
    std::cerr << "  result: state directory looks usable\n";
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
    if (dir_.empty()) {
        std::cerr << "Kurvenrausch: save_choices skipped (empty state dir)\n";
        return;
    }
    if (!make_dir()) return;
    const fs::path file = fs::path(dir_) / "choices", temp = fs::path(dir_) / "choices.tmp";
    {
        std::ofstream out(temp, std::ios::trunc);
        out << format_choices(c);
        out.flush();
        if (!out) {
            fail("Cannot write the choices temp file");
            return;
        }
    }
    std::error_code ec;
    fs::rename(temp, file, ec);
    if (ec) {
        std::cerr << "Kurvenrausch: rename " << temp << " -> " << file << " failed: " << ec.message()
                  << "\n";
        fail("Cannot rename the choices file");
    } else {
        persist();
    }
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
