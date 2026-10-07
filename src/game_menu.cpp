// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

// The pause menu: its pages, built from the game's state, and what the
// player does on them.
//
//   PAUSED      resume, restart, RACE SETUP, OPTIONS, EXTRAS, quit
//   RACE SETUP  track, start in, time of day, weather, traffic, start race
//   OPTIONS     GAMEPLAY  fuel, nitro, police, camera
//               VIDEO     fullscreen, screen, resolution, renderer; HUD,
//                         mini map, mirror, weather FX, FPS
//               AUDIO     sound, effects and music volume, radio
//               CONTROLS  KEYBOARD and GAMEPAD bindings, rumble
//               DEBUG     car, driver, passenger, hour; headlight beam,
//                         present path, sprite viewer
//   EXTRAS      watch the demo (the attract mode), its text and when it
//               starts by itself, LAP RECORDS

#include "game.hpp"

#include "music.hpp"
#include "people.hpp"
#include "sprite_viewer.hpp"
#include "vehicles.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace racer {

namespace {

#ifdef __EMSCRIPTEN__
constexpr bool web = true; // nothing to quit to
#else
constexpr bool web = false;
#endif

// Seconds a rebinding waits for a key before it gives up.
constexpr Uint32 capture_timeout_ms = 8000;

// The items' ids, unique over all pages.
enum Id : int {
    Back,
    // PAUSED
    Resume, Restart, RaceSetup, OptionsPage, ExtrasPage, Quit,
    // RACE SETUP
    TrackPick, ZonePick, TimePick, WeatherPick, TrafficPick, StartRace,
    // OPTIONS
    GameplayPage, VideoPage, AudioPage, ControlsPage, DebugPage,
    // GAMEPLAY
    FuelPick, Nitros, Police, CameraPick,
    // VIDEO
    Fullscreen, Screen, Resolution, UiScale, Renderer, Hud, MiniMap, Mirror, WeatherFx, Fps,
    // AUDIO
    Sound, EffectsVolume, MusicVolume, Radio,
    // CONTROLS
    KeyboardPage, GamepadPage, Rumble, ResetBindings,
    // EXTRAS
    Demo, DemoText, DemoIdle, RecordsPage,
    // LAP RECORDS
    RecordsTrack,
    // DEBUG
    Car, Driver, Passenger, Hour, BeamPick, Present, Sprites,
    // KEYBOARD / GAMEPAD: one per Action from here on
    BindFirst = 1000,
};

int wrap(int v, int n) { return ((v % n) + n) % n; }

const char* on_off(bool on) { return on ? "ON" : "OFF"; }

MenuItem action(int id, std::string label, std::string help = {}) {
    MenuItem it;
    it.id = id;
    it.kind = ItemKind::Action;
    it.label = std::move(label);
    it.help = std::move(help);
    return it;
}

MenuItem submenu(int id, std::string label, std::string help = {}) {
    MenuItem it = action(id, std::move(label), std::move(help));
    it.kind = ItemKind::Submenu;
    return it;
}

MenuItem choice(int id, std::string label, std::string value, std::string help = {}) {
    MenuItem it = action(id, std::move(label), std::move(help));
    it.kind = ItemKind::Choice;
    it.value = std::move(value);
    return it;
}

MenuItem toggle(int id, std::string label, bool on, std::string help = {}) {
    return choice(id, std::move(label), on_off(on), std::move(help));
}

MenuItem slider(int id, std::string label, int level, int levels, std::string help = {}) {
    MenuItem it = action(id, std::move(label), std::move(help));
    it.kind = ItemKind::Slider;
    it.level = level;
    it.levels = levels;
    return it;
}

MenuItem heading(std::string label) {
    MenuItem it;
    it.id = -1;
    it.kind = ItemKind::Heading;
    it.label = std::move(label);
    return it;
}

MenuItem info(std::string label, std::string value) {
    MenuItem it;
    it.id = -1;
    it.kind = ItemKind::Info;
    it.label = std::move(label);
    it.value = std::move(value);
    return it;
}

MenuItem back() { return action(Back, "BACK"); }

// The actions on the binding pages, under their headings.
struct BindGroup {
    const char* title;
    std::vector<Action> actions;
};

const std::vector<BindGroup>& bind_groups() {
    static const std::vector<BindGroup> groups = {
        {"DRIVING",
         {Action::Accelerate, Action::Brake, Action::SteerLeft, Action::SteerRight, Action::Nitro,
          Action::Handbrake, Action::Horn}},
        {"LIGHTS", {Action::Headlights, Action::IndicatorLeft, Action::IndicatorRight, Action::Hazards}},
        {"MORE",
         {Action::Camera, Action::Map, Action::RadioNext, Action::RadioPrevious, Action::Restart, Action::Pause,
          Action::Mute}},
    };
    return groups;
}

std::string hour_label(float hour) {
    char buf[16];
    const int h = static_cast<int>(std::floor(hour)) % 24;
    const int m = static_cast<int>(std::floor(std::fmod(hour, 1.f) * 60.f));
    std::snprintf(buf, sizeof buf, "%02d:%02d", h, m);
    return buf;
}

} // namespace

const std::vector<std::string>& Game::menu_page_names() {
    static const std::vector<std::string> names = {"pause",    "race",     "options", "gameplay",
                                                   "video",    "audio",    "controls", "keyboard",
                                                   "gamepad",  "extras",   "records", "debug"};
    return names;
}

std::string Game::zone_label(const Track& track, int track_index, int index) {
    const Zone& zone = track.zones.at(static_cast<size_t>(index));
    const auto same = std::count_if(track.zones.begin(), track.zones.end(),
                                    [&](const Zone& z) { return z.country == zone.country; });
    return same > 1 && track_index != 0 ? zone.country + " " + zone.region : zone.country;
}

MenuPage Game::menu_page(MenuPageId id) const {
    MenuPage p;
    // "Also on ...": what the first key bound to an action is, for the help lines.
    const auto also = [&](Action a) {
        const int key = bindings_.keys[static_cast<size_t>(a)][0];
        return key ? "ALSO ON " + key_label(key) : std::string();
    };
    switch (id) {
        case MenuPageId::Pause:
            p.title = "PAUSED";
            p.big_title = true;
            p.items = {action(Resume, "RESUME"), action(Restart, "RESTART"), submenu(RaceSetup, "RACE SETUP"),
                       submenu(OptionsPage, "OPTIONS"), submenu(ExtrasPage, "EXTRAS")};
            if (!web) p.items.push_back(action(Quit, "QUIT"));
            break;

        case MenuPageId::Race: {
            p.title = "RACE SETUP";
            const std::string zone = setup_zone_ < static_cast<int>(setup_zones_.size())
                                         ? setup_zones_[static_cast<size_t>(setup_zone_)]
                                         : "-";
            p.items = {
                choice(TrackPick, "TRACK", track_name(setup_track_), "THE ROAD TO DRIVE"),
                choice(ZonePick, "START IN", zone, "WHERE ON THE TRACK TO START"),
                heading("CONDITIONS"),
                choice(TimePick, "TIME OF DAY", time_name(options_.time),
                       options_.time == TimeSetting::Cycle ? "THE DAY PASSES AS YOU DRIVE" : "THE CLOCK STANDS STILL"),
                choice(WeatherPick, "WEATHER", weather_name(options_.weather),
                       options_.weather == WeatherSetting::Changing ? "FRONTS COME AND GO" : ""),
                choice(TrafficPick, "TRAFFIC", traffic_name(options_.traffic)),
                action(StartRace, "START RACE", "TIME OF DAY, WEATHER AND TRAFFIC APPLY NOW"),
                back(),
            };
            break;
        }

        case MenuPageId::Options:
            p.title = "OPTIONS";
            p.items = {submenu(GameplayPage, "GAMEPLAY"), submenu(VideoPage, "VIDEO"), submenu(AudioPage, "AUDIO"),
                       submenu(ControlsPage, "CONTROLS"), submenu(DebugPage, "DEBUG"), back()};
            break;

        case MenuPageId::Gameplay:
            p.title = "GAMEPLAY";
            p.items = {
                toggle(FuelPick, "FUEL", options_.fuel, options_.fuel ? "FILL UP AT GAS STATIONS" : "THE TANK NEVER RUNS DRY"),
                choice(Nitros, "NITRO", std::to_string(options_.nitros), "CANISTERS FOR EACH LAP"),
                toggle(Police, "POLICE", options_.police, "CHASES NOW AND THEN"),
                choice(CameraPick, "CAMERA", view_name(view_mode_), also(Action::Camera)),
                back(),
            };
            break;

        case MenuPageId::Video: {
            p.title = "VIDEO";
            const bool fullscreen = display_ && display_->is_fullscreen();
            std::string renderer = scene_backend_name(scene_backend_);
            const std::string active = use_gles_ ? "GLES" : "SOFTWARE";
            const std::string map = !debug_.map ? "OFF" : map_zoomed_ ? "ZOOMED" : "WHOLE LAP";
            p.items = {
                heading("DISPLAY"),
                action(Fullscreen, fullscreen ? "LEAVE FULLSCREEN" : "ENTER FULLSCREEN", "ALSO ON F11 OR ALT+ENTER"),
                choice(Screen, "SCREEN", wide_ ? "WIDE" : "4:3", wide_ ? "AS WIDE AS THE SCREEN, UP TO 2:1" : "BLACK BARS ON WIDER SCREENS"),
                choice(Resolution, "RESOLUTION", pixel_scale_ >= 2 ? "HD" : "SD",
                       pixel_scale_ >= 2 ? "640 X 480, SMOOTHER EDGES" : "320 X 240, CHUNKY PIXELS"),
                choice(UiScale, "UI SCALE", std::to_string(ui_scale_) + "X",
                       ui_scale_ == 1 ? "SAME SIZE AS SD, EVEN IN HD" : "LARGER MENUS AND HUD"),
                choice(Renderer, "RENDERER", renderer, "RUNNING ON " + active + "  (F8)"),
                heading("HUD"),
                toggle(Hud, "HUD", debug_.hud, "LAP TIMES, SPEED, FUEL AND NITRO"),
                choice(MiniMap, "MINI MAP", map, also(Action::Map)),
                toggle(Mirror, "MIRROR", debug_.mirror, "THE REAR-VIEW MIRROR"),
                toggle(WeatherFx, "WEATHER FX", debug_.weather, "RAIN AND SNOW OVER THE PICTURE"),
                toggle(Fps, "SHOW FPS", debug_.fps, "FRAME RATE AND TIMINGS"),
                back(),
            };
            break;
        }

        case MenuPageId::Audio:
            p.title = "AUDIO";
            p.items = {
                toggle(Sound, "SOUND", !muted_, also(Action::Mute)),
                slider(EffectsVolume, "EFFECTS", engine_vol_, max_volume, "THE ENGINE, THE HORN AND THE WORLD"),
                slider(MusicVolume, "MUSIC", music_vol_, max_volume, "THE RADIO"),
                choice(Radio, "RADIO", Music::name(music_), also(Action::RadioNext)),
                back(),
            };
            break;

        case MenuPageId::Controls: {
            p.title = "CONTROLS";
            const int pads = input_.controller_count();
            const std::string connected = pads == 0   ? "NO CONTROLLER CONNECTED"
                                          : pads == 1 ? "1 CONTROLLER CONNECTED"
                                                      : std::to_string(pads) + " CONTROLLERS CONNECTED";
            p.items = {
                submenu(KeyboardPage, "KEYBOARD", "REBIND THE KEYS"),
                submenu(GamepadPage, "GAMEPAD", connected),
                toggle(Rumble, "RUMBLE", rumble_, "PAD VIBRATION"),
                back(),
            };
            break;
        }

        case MenuPageId::Keyboard:
        case MenuPageId::Gamepad: {
            const bool pad = id == MenuPageId::Gamepad;
            p.title = pad ? "GAMEPAD" : "KEYBOARD";
            p.hint = pad ? "A: CHANGE   X: CLEAR   B: BACK" : "ENTER: CHANGE   DEL: CLEAR   ESC: BACK";
            for (const BindGroup& g : bind_groups()) {
                p.items.push_back(heading(g.title));
                for (Action a : g.actions) {
                    if (pad && a == Action::Pause) continue; // Start, always
                    MenuItem it;
                    it.id = BindFirst + static_cast<int>(a);
                    it.kind = ItemKind::Binding;
                    it.label = action_name(a);
                    const auto& slots = pad ? bindings_.pad[static_cast<size_t>(a)] : bindings_.keys[static_cast<size_t>(a)];
                    it.value = pad ? pad_label(slots[0]) : key_label(slots[0]);
                    it.value2 = pad ? pad_label(slots[1]) : key_label(slots[1]);
                    if (capturing_ && capture_pad_ == pad && capture_action_ == a)
                        it.help = pad ? "PRESS A BUTTON  (START CANCELS)" : "PRESS A KEY  (ESC CANCELS)";
                    p.items.push_back(it);
                }
            }
            p.items.push_back(heading("FIXED"));
            if (pad) {
                p.items.push_back(info("START", "PAUSE, MENU"));
                p.items.push_back(info("D-PAD, A, B", "MENU"));
            } else {
                p.items.push_back(info("ESC", web ? "PAUSE, BACK" : "QUIT, BACK"));
                p.items.push_back(info("F11, ALT+ENTER", "FULLSCREEN"));
                p.items.push_back(info("F8", "RENDERER"));
            }
            p.items.push_back(action(ResetBindings, "RESET TO DEFAULTS", pad ? "THE STANDARD PAD LAYOUT" : "THE STANDARD KEYS"));
            p.items.push_back(back());
            break;
        }

        case MenuPageId::Extras:
            p.title = "EXTRAS";
            p.items = {
                action(Demo, "WATCH DEMO", "FOLLOW THE TRAFFIC, ANY BUTTON ENDS IT"),
                toggle(DemoText, "DEMO TEXT", demo_text_, "THE TITLE AND PROMPT OVER THE DEMO"),
                choice(DemoIdle, "DEMO WHEN IDLE", demo_idle_name(demo_idle_), "STARTS BY ITSELF WHEN NOBODY PLAYS"),
                submenu(RecordsPage, "LAP RECORDS", "THE FASTEST LAPS EVER DRIVEN"),
                back(),
            };
            break;

        case MenuPageId::Records: {
            p.title = "LAP RECORDS";
            p.items.push_back(choice(RecordsTrack, "TRACK", track_name(records_track_)));
            p.items.push_back(heading("FASTEST"));
            if (records_.empty()) p.items.push_back(info("NO LAPS YET", ""));
            for (size_t i = 0; i < records_.size(); ++i) {
                const LapRecord& lap = records_[i];
                p.items.push_back(info(std::to_string(i + 1) + ". " + format_lap_time(lap.seconds),
                                       lap.car + "  " + lap.driver));
            }
            p.items.push_back(back());
            break;
        }

        case MenuPageId::Debug:
            p.title = "DEBUG";
            p.items = {
                heading("CHEATS"),
                choice(Car, "CAR", car_model(car_model_).name),
                choice(Driver, "DRIVER", driver(driver_).name),
                choice(Passenger, "PASSENGER", passenger(passenger_).name),
                choice(Hour, "HOUR", hour_label(hour_), "SETS TIME OF DAY TO CYCLE"),
                heading("RENDERING"),
                toggle(BeamPick, "HEADLIGHT BEAM", debug_.headlights),
                choice(Present, "PRESENT", present_backend_name(present_backend_), "HOW THE PICTURE REACHES THE SCREEN"),
                action(Sprites, "SPRITE VIEWER", "EVERY SPRITE OF THE GAME"),
                back(),
            };
            break;

        default: break;
    }
    return p;
}

void Game::open_menu(MenuPageId id) {
    menu_views_[static_cast<size_t>(id)].reset();
    if (id == MenuPageId::Race) pick_setup_track(track_index_);
    if (id == MenuPageId::Records) {
        records_track_ = track_index_;
        load_records();
    }
    menu_stack_.push_back(id);
}

void Game::close_menu() {
    if (!menu_stack_.empty()) menu_stack_.pop_back();
    if (menu_stack_.empty()) resume();
}

void Game::resume() {
    end_capture();
    menu_stack_.clear();
    paused_ = false;
}

void Game::pick_setup_track(int index) {
    setup_track_ = wrap(index, track_count);
    setup_zones_.clear();
    // The places on another track than the one loaded: it is built for its
    // zones (and thrown away again).
    const Track other = setup_track_ == track_index_ ? Track{} : build_track(setup_track_);
    const Track& t = setup_track_ == track_index_ ? track_ : other;
    for (int i = 0; i < static_cast<int>(t.zones.size()); ++i) setup_zones_.push_back(zone_label(t, setup_track_, i));
    setup_zone_ = setup_track_ == track_index_ ? std::clamp(zone_, 0, std::max(0, static_cast<int>(t.zones.size()) - 1)) : 0;
}

void Game::load_records() {
    records_.clear();
    const std::string name = track_name(records_track_);
    for (const LapRecord& lap : store_.load_laps())
        if (lap.track == name) records_.push_back(lap);
    std::sort(records_.begin(), records_.end(),
              [](const LapRecord& a, const LapRecord& b) { return a.seconds < b.seconds; });
    if (records_.size() > 10) records_.resize(10);
}

void Game::apply_bindings() {
    input_.set_bindings(bindings_);
    input_.set_rumble(rumble_);
}

void Game::start_capture(bool pad, Action a, int slot) {
    capturing_ = true;
    capture_pad_ = pad;
    capture_action_ = a;
    capture_slot_ = slot;
    capture_started_ = SDL_GetTicks();
    input_.set_capture(true);
}

void Game::end_capture() {
    if (!capturing_) return;
    capturing_ = false;
    input_.set_capture(false);
}

void Game::update_capture(const InputState& input) {
    if (input.menu.back || SDL_GetTicks() - capture_started_ > capture_timeout_ms) {
        end_capture();
        return;
    }
    if (!capture_pad_ && input.captured_key != 0) bind_key(bindings_, capture_action_, capture_slot_, input.captured_key);
    else if (capture_pad_ && input.captured_pad != pad_none)
        bind_pad(bindings_, capture_action_, capture_slot_, input.captured_pad);
    else return;
    end_capture();
    apply_bindings();
    save_choices();
}

void Game::draw_menu() {
    const MenuPageId id = menu_stack_.empty() ? MenuPageId::Pause : menu_stack_.back();
    const MenuPage page = menu_page(id);
    MenuView& view = menu_views_[static_cast<size_t>(id)];
    view.settle(page, menu_layout(page, width_, fb_height(), ui_scale_).rows);
    racer::draw_menu(hud_canvas(), page, view, capturing_, std::fmod(clock_ + static_cast<float>(SDL_GetTicks()) / 1000.f, 0.6f) < 0.4f, ui_scale_);
}

bool Game::menu_event(MenuPageId page, const MenuEvent& ev) {
    using T = MenuEvent::Type;
    if (ev.type == T::None) return true;
    if (ev.type == T::Back || (ev.type == T::Activate && ev.id == Back)) {
        close_menu();
        return true;
    }
    const int step = ev.step < 0 ? -1 : 1;
    const Options before = options_;

    // The binding pages.
    if (ev.id >= BindFirst && ev.id < BindFirst + action_count) {
        const auto a = static_cast<Action>(ev.id - BindFirst);
        const bool pad = page == MenuPageId::Gamepad;
        if (ev.type == T::Activate) {
            start_capture(pad, a, ev.slot);
            return true;
        }
        if (ev.type == T::Clear) {
            if (pad) bind_pad(bindings_, a, ev.slot, pad_none);
            else bind_key(bindings_, a, ev.slot, 0);
            apply_bindings();
            save_choices();
        }
        return true;
    }

    switch (ev.id) {
        // PAUSED
        case Resume: resume(); return true;
        case Restart:
            reset();
            resume();
            return true;
        case RaceSetup: open_menu(MenuPageId::Race); return true;
        case OptionsPage: open_menu(MenuPageId::Options); return true;
        case ExtrasPage: open_menu(MenuPageId::Extras); return true;
        case Quit: return false;

        // RACE SETUP
        case TrackPick: pick_setup_track(setup_track_ + step); return true;
        case ZonePick:
            if (!setup_zones_.empty()) setup_zone_ = wrap(setup_zone_ + step, static_cast<int>(setup_zones_.size()));
            return true;
        case TimePick: options_.time = step_setting(options_.time, step); break;
        case WeatherPick: options_.weather = step_setting(options_.weather, step); break;
        case TrafficPick: options_.traffic = wrap(options_.traffic + step, traffic_levels); break;
        case StartRace:
            resume();
            if (setup_track_ != track_index_) load_track(setup_track_);
            else reset();
            if (setup_zone_ > 0) start_at(zone_start_position(setup_zone_));
            save_choices();
            return true;

        // OPTIONS
        case GameplayPage: open_menu(MenuPageId::Gameplay); return true;
        case VideoPage: open_menu(MenuPageId::Video); return true;
        case AudioPage: open_menu(MenuPageId::Audio); return true;
        case ControlsPage: open_menu(MenuPageId::Controls); return true;
        case DebugPage: open_menu(MenuPageId::Debug); return true;

        // GAMEPLAY
        case FuelPick: options_.fuel = !options_.fuel; break;
        case Nitros: options_.nitros = wrap(options_.nitros + step, max_nitros + 1); break;
        case Police: options_.police = !options_.police; break;
        case CameraPick: view_mode_ = static_cast<ViewMode>(wrap(static_cast<int>(view_mode_) + step, view_modes)); break;

        // VIDEO
        case Fullscreen:
            if (display_) display_->toggle_fullscreen();
            break;
        case Screen: wide_ = !wide_; break;
        case Resolution: set_pixel_scale(pixel_scale_ >= 2 ? 1 : 2); break;
        case UiScale: {
            const int next = ui_scale_ + (ev.step >= 0 ? 1 : -1);
            set_ui_scale(next < 1 ? 3 : next > 3 ? 1 : next);
            break;
        }
        case Renderer:
            scene_backend_ = static_cast<SceneBackend>(wrap(static_cast<int>(scene_backend_) + step, 3));
            apply_scene_backend();
            break;
        case Hud: debug_.hud = !debug_.hud; break;
        case MiniMap: {
            // Zoomed, the whole lap, off.
            const int state = !debug_.map ? 2 : map_zoomed_ ? 0 : 1;
            const int next = wrap(state + step, 3);
            debug_.map = next != 2;
            if (next != 2) map_zoomed_ = next == 0;
            break;
        }
        case Mirror: debug_.mirror = !debug_.mirror; break;
        case WeatherFx: debug_.weather = !debug_.weather; break;
        case Fps: debug_.fps = !debug_.fps; break;

        // AUDIO
        case Sound: muted_ = !muted_; break;
        case EffectsVolume: engine_vol_ = std::clamp(engine_vol_ + step, 0, max_volume); break;
        case MusicVolume: music_vol_ = std::clamp(music_vol_ + step, 0, max_volume); break;
        case Radio:
            music_ = step < 0 ? Music::previous(music_) : Music::next(music_);
            synth_.set_music(music_);
            break;

        // CONTROLS
        case KeyboardPage: open_menu(MenuPageId::Keyboard); return true;
        case GamepadPage: open_menu(MenuPageId::Gamepad); return true;
        case Rumble:
            rumble_ = !rumble_;
            apply_bindings();
            if (rumble_) input_.rumble(0.4f, 0.4f, 150); // a taste of it
            break;
        case ResetBindings: {
            const Bindings defaults = default_bindings();
            if (page == MenuPageId::Gamepad) bindings_.pad = defaults.pad;
            else bindings_.keys = defaults.keys;
            apply_bindings();
            break;
        }

        // EXTRAS
        case Demo:
            resume();
            start_attract(true); // the race waits
            attract_armed_ = false;
            return true;
        case DemoText: demo_text_ = !demo_text_; break;
        case DemoIdle: demo_idle_ = wrap(demo_idle_ + step, demo_idle_choices); break;
        case RecordsPage: open_menu(MenuPageId::Records); return true;
        case RecordsTrack:
            records_track_ = wrap(records_track_ + step, track_count);
            load_records();
            return true;

        // DEBUG
        case Car:
            car_model_ = wrap(car_model_ + step, car_models);
            apply_car();
            break;
        case Driver: driver_ = wrap(driver_ + step, drivers); break;
        case Passenger: passenger_ = wrap(passenger_ + step, passengers); break;
        case Hour:
            step_hour(hour_, step);
            options_.time = TimeSetting::Cycle; // so the clock stays where it was put
            break;
        case BeamPick: debug_.headlights = !debug_.headlights; break;
        case Present: {
            const PresentBackend present = next_present_backend(present_backend_);
            if (!display_) break;
            if (display_->set_present_backend(present)) {
                present_backend_ = present;
                gles_.invalidate();
                apply_scene_backend();
            } else {
                present_backend_ = display_->present_backend() == PresentBackend::Gl ? PresentBackend::Gl
                                                                                     : PresentBackend::Sdl;
            }
            break;
        }
        case Sprites:
            if (display_) run_sprite_viewer_session(*display_, input_, sprites_);
            return true;
        default: return true;
    }
    if (ev.id == TimePick || ev.id == WeatherPick || ev.id == TrafficPick || ev.id == FuelPick || ev.id == Nitros ||
        ev.id == Police || ev.id == Hour)
        apply_options(before); // saves when they changed
    save_choices();
    return true;
}

} // namespace racer
