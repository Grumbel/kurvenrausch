// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "game.hpp"
#include "sprite_viewer.hpp"

#include "drivetrain.hpp"

#include <SDL2/SDL.h>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <iostream>

namespace racer {

namespace {

// Traffic density: one vehicle per this many segments of the lap.
constexpr int segments_per_vehicle = 60;
constexpr float oncoming_share = 0.4f; // of the traffic comes the other way
constexpr float oncoming_dodge = 0.3f; // an oncoming car meeting the player in its lane swerves this far out ...
constexpr float oncoming_alarm = 25.f; // ... from this many segments away

// Honking: cars this far ahead (in segments) and this close to the player's
// line (road half-widths) pull over; they swerve faster for a while.
constexpr float honk_range = 60.f;
constexpr float honk_clearance = 0.45f;
constexpr float startled_seconds = 1.5f;

// Traffic following a slower vehicle in its lane: it looks this many segments
// ahead, keeps a gap of follow_gap segments, may close in at follow_closing
// units per second per unit of gap above that, and treats anything within
// follow_width road half-widths as in its lane. Braking and accelerating are
// fractions of the player's top speed per second.
// Traffic keeps a gap of about a car and a half to the vehicle ahead (a car
// is some 5 segments long), slowing down from follow_range away.
constexpr float follow_range = 16.f;
constexpr float follow_gap = 8.f;
constexpr float follow_closing = 1.5f;
constexpr float follow_width = 0.3f;
constexpr float traffic_brake = 0.6f;
constexpr float traffic_accel = 0.15f;

// Stuck behind something slower with no lane of its own free to pass in, a car
// overtakes in the oncoming lane when that is clear far enough ahead for the
// whole manoeuvre: it needs to gain overtake_length segments on the vehicle, at
// overtake_boost of top speed over it at most (and overtake_top in all), against
// oncoming traffic at up to overtake_oncoming. It doesn't bother for less than
// overtake_min_gain of top speed, and heads back early with oncoming traffic
// overtake_abort segments away.
constexpr float overtake_length = 2.f * follow_gap + 8.f;
constexpr float overtake_boost = 0.15f;
constexpr float overtake_top = 0.75f;
constexpr float overtake_oncoming = 0.6f;
constexpr float overtake_min_gain = 0.05f;
constexpr float overtake_abort = 80.f;

// Above the top speed (after nitro or a pass boost) the car loses this much
// of the top speed per second until it is back down.
constexpr float overspeed_drag = 0.15f;

// Above this fraction of the top speed the tyres aquaplane in a wet spot and
// keep only this much of their grip.
constexpr float aquaplane_speed = 0.35f;

// How much of the car's height above the road in a jump the camera follows.
constexpr float camera_air_share = 0.7f;

// At a fork the roads overlap while their centres are less than this many
// road half-widths apart; the view turns to a newly taken road's heading
// with this time constant (seconds).
constexpr float fork_overlap = 1.5f;
constexpr float view_turn_seconds = 0.35f;
constexpr float fork_bend_push = 0.3f;
constexpr float aquaplane_grip = 0.35f;

// On an oil slick above this fraction of the top speed the tyres keep only
// this much of their grip.
constexpr float oil_speed = 0.15f;
constexpr float oil_grip = 0.08f;

// The handbrake, pulled above this fraction of the top speed: the grip left
// for the tail, how much harder the steering bites, and how strongly it
// slows the car (of the foot brake).
constexpr float handbrake_speed = 0.1f;
constexpr float handbrake_grip = 0.5f;
constexpr float handbrake_steer = 1.7f;
constexpr float handbrake_decel = 0.6f;

// Reverse gear: top speed and acceleration, of the forward ones.
constexpr float reverse_top = 0.15f;
constexpr float reverse_accel = 0.4f;

// Refuelling works on the forecourt below this fraction of the top speed.
#ifdef __EMSCRIPTEN__
constexpr bool web = true; // running in a web page
#else
constexpr bool web = false;
#endif
constexpr float refuel_speed = 0.08f;
constexpr float nitro_fill_seconds = 1.f; // a canister filled at the chemical plant
constexpr float offer_speed = 0.01f; // a lot offers its choice below this speed (of top speed)
// Fares (see update_fares()): how many wait ahead at a time, how far ahead
// they turn up and are forgotten behind (segments), how near the car must
// stop, how slowly (of top speed), and how far out at the kerb they stand.
constexpr size_t hails_waiting = 2;
constexpr float hail_ahead_min = 120.f, hail_ahead_max = 260.f, hail_behind = 20.f;
constexpr float hail_reach = 4.f;
constexpr float fare_stop_speed = 0.02f;
constexpr float hail_kerb = 1.06f;
constexpr float progress_save_seconds = 30.f; // how often the position is saved
// The movie cars (see update_movie_cars()).
constexpr float mph88_low = 139.f, mph88_high = 145.f; // km/h: 88 mph, about
constexpr float mph88_seconds = 4.f;
constexpr float night_run_seconds = 10.f;
constexpr float scanner_jump = 9000.f; // world units a second up
// Trains (see update_train()): decided for a crossing this many segments
// ahead, this often, at this speed (road half-widths a second); one car is
// this long (world units); its tail clears the road this long before the
// car would get there.
constexpr float train_decide_min = 120.f, train_decide_max = 260.f;
constexpr float train_chance = 0.6f;
constexpr float train_speed = 6.f;
constexpr float train_car_length = 3200.f;
constexpr float train_margin = 0.4f;
// Animals (see update_animals()): they turn up this many segments ahead,
// this far out to the side (road half-widths), and are forgotten this far
// behind.
constexpr float animal_ahead = 140.f, animal_start = 1.5f, animal_behind = 20.f;
constexpr float attract_idle_seconds = 120.f;  // without input, back to the attract mode
constexpr float attract_follow_seconds = 20.f; // each car followed this long

// The cars a lot sells.
CarRange lot_range(Lot lot) {
    return lot == Lot::SportsDealer ? CarRange::Sports : lot == Lot::Truckstop ? CarRange::Trucks : CarRange::Regular;
}
// Stranded with an empty tank this long, the driver pours in a spare can.
constexpr float stranded_seconds = 3.f;
constexpr float spare_can = 0.15f;

// Hitting something solid faster than this fraction of the top speed is a
// crash with the tumbling animation; slower it is a knock that slows the car.
constexpr float crash_speed = 0.4f;
// Where the car's tyres touch the ground on screen, for dust and debris.

// The mirror's camera sits in the car, lower than the chase camera, and sees
// a narrower field than the main view.
constexpr float mirror_camera_height = 700.f;
constexpr float mirror_depth = 1.2f;
constexpr float mirror_horizon = 13.f; // screen row in the mirror

// Housing around the mirror glass, with the stem holding it from the roof.
void draw_mirror_frame(Framebuffer& fb, int x, int y, int w, int h) {
    const Color dark{0x16, 0x16, 0x1a}, body{0x2e, 0x2e, 0x34}, light{0x50, 0x50, 0x58};
    fb.fill_rect(x + w / 2 - 4, 0, 8, y, dark);
    fb.fill_rect(x + w / 2 - 3, 0, 6, y, body);
    fb.fill_rect(x + w / 2 - 2, 0, 1, y, light);
    // Rounded corners: each ring is inset by one pixel on its first and last row.
    auto ring = [&](int inset, Color c) {
        const int x0 = x - inset, y0 = y - inset, x1 = x + w + inset, y1 = y + h + inset;
        fb.hline(x0 + 2, x1 - 2, y0, c);
        fb.hline(x0 + 1, x1 - 1, y0 + 1, c);
        fb.fill_rect(x0, y0 + 2, x1 - x0, y1 - y0 - 4, c);
        fb.hline(x0 + 1, x1 - 1, y1 - 2, c);
        fb.hline(x0 + 2, x1 - 2, y1 - 1, c);
    };
    ring(4, dark);
    ring(3, body);
    fb.hline(x - 1, x + w + 1, y - 3, light); // lit top edge
}

// Faint diagonal reflections across the glass.
void draw_mirror_sheen(Framebuffer& fb, int x, int y, int w, int h) {
    constexpr int streaks[][2] = {{0, 6}, {9, 2}}; // first column at the bottom, width
    for (int row = 0; row < h; ++row) {
        for (const auto& streak : streaks) {
            for (int i = 0; i < streak[1]; ++i) {
                const int col = w / 5 + streak[0] + i + (h - 1 - row);
                if (col < w) fb.blend_pixel(x + col, y + row, Color{255, 255, 255}, 0.12f);
            }
        }
    }
}

// Nitro flame bursting out of the exhaust pipe at (x, y) on `side` (-1 left,
// +1 right): a white-hot core in an orange glow, with flickering tongues
// flaring outwards and upwards (the pipes sit too low on the screen for a
// flame streaming towards the camera), and a few sparks.
void draw_flame(Framebuffer& fb, float x, float y, int side, float intensity, uint32_t& rng) {
    auto random = [&rng] {
        rng = rng * 1664525u + 1013904223u;
        return static_cast<float>(rng >> 8) / 16777216.f;
    };
    const Color hot{235, 245, 255}, yellow{255, 220, 90}, orange{255, 120, 30};
    const float r = (3.f + 1.5f * random()) * intensity;
    for (int py = static_cast<int>(y - 2.f * r); py <= static_cast<int>(y + 2.f * r); ++py) {
        for (int px = static_cast<int>(x - 2.f * r); px <= static_cast<int>(x + 2.f * r); ++px) {
            const float d = std::hypot(static_cast<float>(px) + 0.5f - x, static_cast<float>(py) + 0.5f - y) / r +
                            0.3f * (bayer4(px, py) - 0.5f);
            if (d < 1.f) fb.put_pixel(px, py, d < 0.45f ? hot : d < 0.75f ? yellow : orange);
            else if (d < 2.f) fb.blend_pixel(px, py, orange, 0.4f * (2.f - d) * intensity);
        }
    }
    // Tongues: tapering streaks from the core, fanning out from the side the
    // pipe is on over to straight up.
    for (int i = 0; i < 4; ++i) {
        const float angle = static_cast<float>(side) * (0.25f + 1.1f * random()) * PI / 2.f;
        const float dx = std::sin(angle), dy = -std::cos(angle) * 0.7f;
        const float len = r * (1.5f + 2.f * random());
        for (float t = 0.6f * r; t < len; t += 0.5f) {
            const float f = t / len;
            fb.put_pixel(static_cast<int>(x + dx * t), static_cast<int>(y + dy * t),
                         f < 0.4f ? yellow : f < 0.8f ? orange : Color{200, 60, 20});
        }
    }
    for (int i = 0; i < 3; ++i) {
        const int px = static_cast<int>(x + static_cast<float>(side) * random() * 5.f * r);
        const int py = static_cast<int>(y - random() * 3.f * r);
        fb.put_pixel(px, py, Color{255, 200, 90});
    }
}

// Do the intervals [c1 - w1/2, c1 + w1/2] and [c2 - w2/2, c2 + w2/2] overlap?
bool overlap(float c1, float w1, float c2, float w2) {
    return std::abs(c1 - c2) * 2.f < w1 + w2;
}

} // namespace

Game::Game()
    : fb_(base_width, height), track_(build_track(0)), mirror_fb_(mirror_width, mirror_height),
      weather_(base_width, height) {
    // Framebuffers match pixel_scale_ once set_pixel_scale runs; start at SD.
    for (int k = 0; k < lot_kinds; ++k) lots_[static_cast<size_t>(k)] = track_.lots(static_cast<Lot>(k));
    map_ = track_map(track_);
    player_ = world_.create();
    world_.add<Transform>(player_);
    world_.add<Velocity>(player_);
    world_.add<Player>(player_, Player::for_segment_length(track_.segment_length));
    base_max_speed_ = world_.get<Player>(player_).max_speed;
    apply_car();

    camera_ = world_.create();
    world_.add<Camera>(camera_);

    spawn_traffic();
}

void Game::spawn_traffic() {
    std::vector<Entity> old;
    world_.view<Traffic>([&](Entity e, Traffic&) { old.push_back(e); });
    for (Entity e : old) world_.destroy(e);

    const float max_speed = base_max_speed_;
    const float seg_len = track_.segment_length;
    const float n = static_cast<float>(track_.segments.size());
    uint32_t seed = 0x7a3c9e11u;
    auto rnd = [&seed] {
        seed = seed * 1664525u + 1013904223u;
        return static_cast<float>(seed >> 8) / 16777216.f;
    };
    const int traffic_count = static_cast<int>(static_cast<float>(track_.segments.size()) / segments_per_vehicle *
                                               traffic_factor(options_.traffic));
    for (int i = 0; i < traffic_count; ++i) {
        const Entity car = world_.create();
        // Keep the start straight clear.
        const float segment = 60.f + rnd() * (n - 80.f);
        const RoadTheme& look = track_.look(static_cast<int>(segment));
        Traffic traffic;
        traffic.kind = traffic_vehicle(rnd());
        // Some come the other way, in their lane; the rest pick one of the
        // lanes going the player's way. Rivals race the player's way.
        if (rnd() < oncoming_share && traffic.kind != Vehicle::Rival) traffic.dir = -1;
        const int own = nearest_own_lane(look.lanes, look.left_hand, rnd() * 2.f - 1.f);
        const float lane = lane_center(look.lanes, traffic.dir < 0 ? oncoming_lane(look.lanes, look.left_hand) : own);
        world_.add<Transform>(car, Transform{lane, 0.f, segment * seg_len});
        const VehicleInfo& info = vehicle_info(traffic.kind);
        const float speed = max_speed * (info.min_speed + (info.max_speed - info.min_speed) * rnd());
        world_.add<Velocity>(car, Velocity{speed});
        traffic.style = i % info.styles;
        traffic.target_x = lane;
        traffic.cruise = speed;
        world_.add<Traffic>(car, traffic);
    }
    // One truck in the USA drives with its ramp down.
    bool ramp = false;
    world_.view<Transform, Traffic>([&](Entity, Transform& t, Traffic& traffic) {
        if (!ramp && traffic.kind == Vehicle::Truck && traffic.dir > 0 && track_.zone_at(t.z).country == "USA") {
            traffic.ramp = ramp = true;
        }
    });
}

bool Game::init(bool fullscreen) {
    // Last run's choices first: fullscreen and HD affect the window and the
    // framebuffer size. A CLI --fullscreen still forces fullscreen on.
    store_ = Store(user_state_dir());
    int last_track = 0; // the track driven last
    int saved_hd = 0;
    if (const std::optional<Choices> c = store_.load_choices()) {
        const auto wrap = [](int i, int n) { return ((i % n) + n) % n; };
        car_model_ = wrap(c->car, car_models);
        driver_ = wrap(c->driver, drivers);
        passenger_ = c->passenger >= 0 && c->passenger < motel_passengers ? c->passenger : nobody;
        view_mode_ = static_cast<ViewMode>(wrap(c->view, view_modes));
        music_ = c->music >= 0 ? c->music % Music::tracks : -1;
        engine_vol_ = std::clamp(c->engine_vol, 0, max_volume);
        music_vol_ = std::clamp(c->music_vol, 0, max_volume);
        wide_ = c->wide != 0;
        muted_ = c->muted != 0;
        present_backend_ = static_cast<PresentBackend>(std::clamp(c->present, 0, 2));
        saved_hd = c->hd != 0 ? 2 : 1;
        if (c->fullscreen) fullscreen = true;
        resume_position_ = c->position;
        resume_minutes_ = c->minutes;
        resume_tank_ = c->tank;
        last_track = c->track;
        const Options before = options_;
        options_ = c->options;
        apply_options(before);
        apply_car();
    }

    display_ = std::make_unique<Display>();
    // GLES scene path needs a GL context; prefer GL present (falls back to SDL).
    if (present_backend_ == PresentBackend::Sdl) present_backend_ = PresentBackend::Auto;
    if (!display_->init("Kurvenrausch", app_id, base_width, height, window_scale, fullscreen, present_backend_))
        return false;
    apply_scene_backend();

    const Bitmap icon = make_app_icon();
    display_->set_icon(icon.px.data(), icon.w, icon.h);
    if (saved_hd > 1) set_pixel_scale(saved_hd);

    if (last_track != 0) load_track(last_track);
    record_lap_ = best_lap(store_.load_laps(), track_name(track_index_));
    best_lap_ = record_lap_;

    synth_.set_music(music_);

    input_.init(); // not fatal: the keyboard always works
    audio_.init(synth_); // nor is a missing audio device

    start_attract(); // until somebody presses something

    std::cout << "Kurvenrausch: " << track_.segments.size() << " segments, "
              << track_.length() << " units.\n"
              << "Controls: Arrows / WASD or gamepad to drive, P / Start to pause,\n"
              << "          R restart, M mute, F8 renderer, F11 fullscreen, Esc to " << (web ? "pause" : "quit") << ".\n";
    return true;
}

void Game::reset() {
    world_.get<Transform>(player_) = Transform{};
    world_.get<Velocity>(player_) = Velocity{};
    background_.reset();
    weather_.reset();
    spawn_traffic();
    race_started_ = false;
    lap_ = 0;
    lap_time_ = last_lap_ = 0.f;
    best_lap_ = record_lap_;
    message_.clear();
    message_time_ = 0.f;
    zone_ = -1;
    banner_time_ = 0.f;
    horn_ = false;
    window_wake_time_ = 0.f;
    nitro_.reset();
    nitro_held_ = false;
    wave_time_ = 0.f;
    crash_time_ = -1.f;
    particles_.clear();
    wet_ = aquaplaning_ = oily_ = handbraking_ = reverse_armed_ = false;
    spin_time_ = 0.f;
    place_on_road(vertical_, track_.height_at(world_.get<Camera>(camera_).player_z()));
    landing_time_ = 0.f;
    view_yaw_ = view_shift_ = 0.f;
    hour_ = start_hour;
    advance_clock(0.f);
    headlights_ = hazards_ = blink_on_ = beacon_ = false;
    hails_.clear();
    crossings_.clear();
    crossing_wait_ = animal_min_wait;
    train_ = Train{};
    train_decided_ = -1;
    if (fare_zone_ >= 0) passenger_ = nobody;
    fare_zone_ = -1;
    fares_paid_ = 0;
    signal_ = 0;
    police_ = INVALID_ENTITY; // gone with the traffic
    chase_ = Chase{};
    chase_cooldown_ = chase_cooldown;
    pulled_over_ = siren_ = 0.f;
    front_.reset();
    flash_time_ = 0.f;
    thunder_delay_ = -1.f;
    fuel_.reset();
    engine_on_ = true;
    refuelling_ = false;
    stranded_time_ = 0.f;
    dirt_.reset();
    washing_ = false;
    bandaged_ = false;
}

// Where lap times and choices are kept: the XDG state directory, except on
// systems without one (Windows, Android), where SDL knows the place for an
// application's files (AppData, the app's internal storage).
std::string Game::user_state_dir() {
#if defined(_WIN32) || defined(__ANDROID__)
    char* path = SDL_GetPrefPath("grumbel", "kurvenrausch");
    if (!path) return {};
    std::string dir = path;
    SDL_free(path);
    while (!dir.empty() && (dir.back() == '/' || dir.back() == '\\')) dir.pop_back();
    return dir;
#else
    return state_dir(std::getenv("XDG_STATE_HOME"), std::getenv("HOME"));
#endif
}

void Game::update_rumble() {
    const auto& vel = world_.get<Velocity>(player_);
    const auto& tr = world_.get<Transform>(player_);
    const float speed_pct = std::abs(vel.speed) / world_.get<Player>(player_).max_speed;
    if (crashed_) {
        input_.rumble(1.f, 0.8f, 250);
    } else if (passed_) {
        input_.rumble(0.2f, 0.6f, 120);
    } else if (handbraking_) {
        input_.rumble(0.3f, 0.5f, 60);
    } else if (oily_) {
        input_.rumble(0.2f, 0.7f, 60);
    } else if (aquaplaning_) {
        input_.rumble(0.1f, 0.4f, 60);
    } else if (nitro_.burning()) {
        input_.rumble(0.3f * nitro_.intensity(), 0.5f * nitro_.intensity(), 60);
    } else if (scraping_ && speed_pct > 0.05f) {
        input_.rumble(0.6f, 0.6f, 60);
    } else if (std::abs(tr.x) > 1.f && speed_pct > 0.05f &&
               !track_.on_forecourt(tr.z + world_.get<Camera>(camera_).player_z(), tr.x)) {
        input_.rumble(0.35f * speed_pct, 0.1f, 60); // rattling along the verge
    }
}

void Game::show_message(std::string text, float seconds) {
    message_ = std::move(text);
    message_time_ = seconds;
}

void Game::run() {
    prev_counter_ = SDL_GetPerformanceCounter();
#ifdef __EMSCRIPTEN__
    // The browser calls a frame per display refresh; nothing to quit to.
    emscripten_set_main_loop_arg([](void* game) { static_cast<Game*>(game)->frame(); }, this, 0, true);
#else
    while (frame()) {
        if (!display_->vsync()) SDL_Delay(1);
    }
    save_choices(); // where the race left off
#endif
}

bool Game::frame() {
    const Uint64 now = SDL_GetPerformanceCounter();
    const double freq = static_cast<double>(SDL_GetPerformanceFrequency());
    const float dt = std::min(static_cast<float>((now - prev_counter_) / freq), 0.25f);
    prev_counter_ = now;
    if (dt > 0.f) fps_ = fps_ * 0.9f + (1.f / dt) * 0.1f;

    InputState& input = input_state_;
    input_.poll(input);
    if (input.quit && !web) return false;
    if (input.toggle_fullscreen) {
        display_->toggle_fullscreen();
        save_choices();
    }
    if (input.toggle_mute) {
        muted_ = !muted_;
        save_choices();
    }
    if (input.toggle_map) map_zoomed_ = !map_zoomed_;

    // The attract mode until somebody presses something; and back to it
    // after a while without anybody at the controls.
    if (attract_) {
        if (input.any_input) {
            leave_attract();
        } else {
            accumulator_ += dt;
            while (accumulator_ >= fixed_dt_) {
                update_attract(fixed_dt_);
                accumulator_ -= fixed_dt_;
            }
        }
        set_width(screen_width());
        render();
        present();
        return true;
    }
    idle_ = input.any_input ? 0.f : idle_ + dt;
    // Now and then the position, for going on from there should the game
    // not be quit properly (a phone closing it, a crash).
    progress_saved_ += dt;
    if (progress_saved_ > progress_save_seconds && !paused_) {
        progress_saved_ = 0.f;
        save_choices();
    }
    if (idle_ > attract_idle_seconds) {
        start_attract(true); // the race waits
        return true;
    }

    if (!paused_) switch_lights(input); // paused, the D-pad moves the menu
    if (input.toggle_renderer) {
        scene_backend_ = next_scene_backend(scene_backend_);
        apply_scene_backend();
        std::cout << "Kurvenrausch: scene renderer " << scene_backend_name(scene_backend_)
                  << (use_gles_ ? " (GLES active)" : " (software active)") << "\n";
    }
    if (input.change_view && !paused_) {
        view_mode_ = static_cast<ViewMode>((static_cast<int>(view_mode_) + 1) % view_modes);
        show_message(view_name(view_mode_), 1.f);
        save_choices();
    }
    if (input.change_music != 0) {
        music_ = input.change_music > 0 ? Music::next(music_) : Music::previous(music_);
        synth_.set_music(music_);
        show_message(Music::name(music_), 1.5f);
        save_choices();
    }
    // The touch screen: fingers onto the screen, the controls into the
    // input (the menu takes the taps when paused). The controls cover the
    // whole screen, the bars beside the picture too.
    const SDL_Rect screen = display_->screen(), picture = display_->picture();
    touch_.set_layout(TouchLayout{static_cast<float>(screen.w), static_cast<float>(screen.h),
                                  static_cast<float>(picture.x), static_cast<float>(picture.y),
                                  static_cast<float>(picture.w), static_cast<float>(picture.h)});
    std::vector<Finger> fingers;
    for (const Finger& f : input.fingers) {
        Finger p = f;
        display_->touch_to_screen(f.x, f.y, p.x, p.y);
        fingers.push_back(p);
    }
    touch_taps_.clear();
    for (const Finger& f : input.taps) {
        Finger p = f;
        display_->touch_to_screen(f.x, f.y, p.x, p.y);
        touch_taps_.push_back(p);
    }
    if (!fingers.empty() || !touch_taps_.empty()) touch_seen_ = true;
    // A tap that came and went between two frames still presses what it hit.
    for (const Finger& tap : touch_taps_) {
        if (std::none_of(fingers.begin(), fingers.end(), [&](const Finger& f) { return f.id == tap.id; })) {
            fingers.push_back(tap);
        }
    }
    if (!paused_) apply_touch(input, fingers);
    if (!update_pause(input)) return false;

    crashed_ = false;
    passed_ = false;
    if (paused_) {
        accumulator_ = 0.f;
    } else {
        accumulator_ += dt;
        while (accumulator_ >= fixed_dt_) {
            fixed_update(input, fixed_dt_);
            accumulator_ -= fixed_dt_;
        }
        update_rumble();
    }

    set_width(screen_width());
    render();
    if (paused_ && options_open_)
        draw_options_menu(fb_, options_menu_, options_, zone_label(options_menu_.zone),
                          track_name(options_menu_.track));
    else if (paused_ && video_open_)
        draw_video_menu(fb_, video_menu_, wide_, pixel_scale_ >= 2, present_backend_, debug_);
    else if (paused_ && audio_open_) draw_audio_menu(fb_, audio_menu_, muted_, engine_vol_, music_vol_, music_);
    else if (paused_ && debug_open_)
        draw_debug_menu(fb_, debug_menu_, debug_, hour_, car_model_, driver_, passenger_);
    else if (paused_) draw_pause_menu(fb_, menu_, zone_label(zone_), track_name(track_index_));
    if (debug_.fps) draw_fps(fb_, fps_);
    present();
    return true;
}

void Game::apply_options(const Options& before) {
    nitro_.set_capacity(options_.nitros);
    front_.force(weather_force(options_.weather));
    advance_clock(0.f);
    if (!options_.fuel) fuel_.reset(); // a full tank, for good
    if ((!options_.police || options_.traffic != before.traffic) && police_ != INVALID_ENTITY) {
        pulled_over_ = 0.f;
        end_chase();
    }
    if (options_.traffic != before.traffic) spawn_traffic();
    if (options_.time != before.time || options_.fuel != before.fuel || options_.nitros != before.nitros ||
        options_.police != before.police || options_.weather != before.weather || options_.traffic != before.traffic) {
        save_choices();
    }
}

void Game::advance_clock(float dt) {
    const float fixed = fixed_hour(options_.time);
    hour_ = fixed >= 0.f ? fixed : advance_hour(hour_, dt);
}


void Game::apply_scene_backend() {
    const bool was = use_gles_;
    use_gles_ = false;
    if (scene_backend_ == SceneBackend::Software) {
        if (was) std::cout << "Kurvenrausch: software scene renderer\n";
        return;
    }
    if (!display_ || !display_->is_gl()) {
        if (scene_backend_ == SceneBackend::Gles)
            std::cout << "Kurvenrausch: GLES scene requested but no GL present path; software\n";
        else if (!was)
            std::cout << "Kurvenrausch: software scene renderer (no GL)\n";
        return;
    }
    if (!display_->make_gl_current()) {
        std::cout << "Kurvenrausch: software scene renderer (make_gl_current failed)\n";
        return;
    }
    // Context may have been rebuilt; always drop names and recreate.
    gles_.invalidate();
    if (!gles_.init()) {
        std::cout << "Kurvenrausch: software scene renderer (GLES init failed)\n";
        return;
    }
    gles_.set_size(width_, fb_height());
    use_gles_ = true;
    if (!was) std::cout << "Kurvenrausch: GLES2 scene renderer\n";
}

void Game::set_width(int w) {
    w = std::clamp(w, fb_base_width(), fb_max_width());
    if (w == width_ && fb_.height() == fb_height()) return;
    if (display_ && !display_->resize_framebuffer(w, fb_height())) return;
    width_ = w;
    fb_ = Framebuffer(width_, fb_height());
    weather_.resize(width_, fb_height());
    // resize_framebuffer rebuilds the GL present context — scene GL objects are stale.
    if (scene_backend_ != SceneBackend::Software) {
        gles_.invalidate();
        apply_scene_backend();
    }
}

void Game::set_pixel_scale(int scale) {
    scale = scale >= 2 ? 2 : 1;
    if (scale == pixel_scale_) return;
    pixel_scale_ = scale;
    width_ = 0; // force set_width to rebuild
    set_width(screen_width());
    mirror_fb_ = Framebuffer(mir_width(), mir_height());
}

int Game::screen_width() const {
    if (!wide_) return fb_base_width();
    // Rounded to an even width, so the picture's centre is between pixels
    // as it is at 4:3.
    const SDL_Rect s = display_->screen();
    return 2 * static_cast<int>(std::lround(static_cast<float>(fb_height()) * static_cast<float>(s.w) /
                                            static_cast<float>(s.h) / 2.f));
}

void Game::draw_touch() {
    overlay_.clear();
    if (touch_seen_ && !paused_ && !attract_) touch_.draw(overlay_);
}

void Game::present() {
    draw_touch();
    if (use_gles_ && !paused_ && !options_open_ && !video_open_ && !audio_open_ && !debug_open_ &&
        gles_.color_texture()) {
        display_->present_gles_scene(gles_.color_texture(), gles_.texture_width(), gles_.texture_height(),
                                     fb_.pixels(), overlay_);
    } else {
        display_->present(fb_.pixels(), overlay_);
    }
}

void Game::switch_lights(const InputState& input) {
    const auto& tr = world_.get<Transform>(player_);
    if (input.toggle_headlights) headlights_ = !headlights_;
    if (input.toggle_hazards) {
        // On a police car (or an ambulance) the switch works the lightbar
        // and siren instead.
        if (body_has_lightbar(car_model(car_model_).body)) beacon_ = !beacon_;
        else hazards_ = !hazards_;
    }
    // An indicator goes off when pressed again, and over to the other side.
    for (int side : {-1, 1}) {
        if (!(side < 0 ? input.signal_left : input.signal_right)) continue;
        signal_ = signal_ == side ? 0 : side;
        signal_x_ = tr.x;
    }
}

// The indicators blink (and the relay ticks) as long as one is on or the
// hazard lights are. An indicator switches itself off once the car has
// moved over (half a road width) its way and the wheel is straight again.
void Game::update_indicators(const InputState& input, float dt) {
    (void)dt;
    const auto& tr = world_.get<Transform>(player_);
    if (signal_ != 0 && (tr.x - signal_x_) * static_cast<float>(signal_) > 0.5f && std::abs(input.steer) < 0.1f) {
        signal_ = 0;
    }
    const bool blinking = signal_ != 0 || hazards_;
    const bool on = blinking && std::fmod(clock_, 0.7f) < 0.4f;
    if (on != blink_on_ && (blinking || blink_on_)) synth_.trigger_tick(on);
    blink_on_ = on;
}

int Game::shown_signal() const {
    if (!blink_on_) return 0;
    return hazards_ ? hazard_signal : signal_;
}

void Game::start_attract(bool keep_race) {
    if (keep_race) {
        // Idle a while: the race waits, saved too, while the attract mode
        // shows the traffic.
        save_choices();
        held_race_ = HeldRace{world_.get<Transform>(player_), hour_, true};
    } else {
        held_race_.held = false;
        reset();
    }
    attract_ = true;
    paused_ = false;
    played_view_ = view_mode_;
    attract_cars_ = 0;
    attract_car_ = INVALID_ENTITY;
    follow_next_car();
}

void Game::leave_attract() {
    attract_ = false;
    view_mode_ = played_view_;
    idle_ = 0.f;
    if (held_race_.held) {
        // Back to the race it interrupted, where the car stood.
        held_race_.held = false;
        world_.get<Transform>(player_) = held_race_.at;
        world_.get<Velocity>(player_).speed = 0.f;
        hour_ = held_race_.hour;
        const float car_z = held_race_.at.z + world_.get<Camera>(camera_).player_z();
        place_on_road(vertical_, track_.height_at(car_z));
        zone_ = -1;
        return;
    }
    reset(); // at the start line ...
    // ... or where the last run left off.
    if (resume_position_ >= 0) {
        start_at(static_cast<float>(resume_position_));
        if (resume_minutes_ >= 0 && options_.time == TimeSetting::Cycle) {
            hour_ = static_cast<float>(resume_minutes_ % (24 * 60)) / 60.f;
        }
        if (resume_tank_ >= 0 && options_.fuel) fuel_.set(static_cast<float>(resume_tank_) / 1000.f);
        resume_position_ = resume_minutes_ = resume_tank_ = -1;
    }
}

// A regular car of the traffic (a car, van or truck), not the one followed
// so far; the view alternates between chase and far.
void Game::follow_next_car() {
    std::vector<Entity> cars;
    world_.view<Traffic>([&](Entity e, Traffic& t) {
        if (e != attract_car_ && t.dir > 0 && t.kind != Vehicle::Rival && t.kind != Vehicle::Police) {
            cars.push_back(e);
        }
    });
    if (cars.empty()) return;
    rng_ = rng_ * 1664525u + 1013904223u;
    attract_car_ = cars[(rng_ >> 8) % cars.size()];
    view_mode_ = attract_cars_++ % 2 ? ViewMode::Far : ViewMode::Chase;
    attract_switch_ = attract_follow_seconds;
    zone_ = -1; // announce where it is
}

// The world goes on without the player: the traffic drives, the camera (the
// player's car, hidden) stays behind the followed car, the weather and the
// time of day move on. No laps, no police, no collisions.
void Game::update_attract(float dt) {
    attract_switch_ -= dt;
    if (attract_switch_ <= 0.f || attract_car_ == INVALID_ENTITY || !world_.has<Transform>(attract_car_)) {
        follow_next_car();
    }
    if (attract_car_ == INVALID_ENTITY) return;
    update_traffic(dt);
    const auto& t = world_.get<Transform>(attract_car_);
    const float speed = world_.get<Velocity>(attract_car_).speed;
    auto& tr = world_.get<Transform>(player_);
    auto& vel = world_.get<Velocity>(player_);
    const float player_z = world_.get<Camera>(camera_).player_z();
    const float prev_z = tr.z;
    tr.z = track_.wrap(t.z - player_z);
    tr.x = t.x;
    vel.speed = speed;
    tr.y = track_.height_at(t.z);
    place_on_road(vertical_, tr.y);

    const Segment& seg = track_.segment_at(t.z);
    const RoadTheme look = look_at(t.z);
    const float speed_pct = speed / base_max_speed_;
    background_.update(seg.curve, track_.wrap(tr.z - prev_z) / track_.segment_length, dt);
    weather_.update(look.rain, look.snowfall, -seg.curve * 25.f * speed_pct, speed_pct, dt);
    front_.update(dt);
    clock_ += dt;
    advance_clock(dt);
    const int zone = track_.zone_number_at(t.z);
    if (zone != zone_) {
        zone_ = zone;
        banner_time_ = 4.f;
    }
    if (banner_time_ > 0.f) banner_time_ -= dt;
    InputState cruising;
    cruising.throttle = 0.5f;
    update_audio(cruising, dt);
}

void Game::apply_touch(InputState& input, const std::vector<Finger>& fingers) {
    const TouchInput t = touch_.update(fingers);
    input.steer = std::clamp(input.steer + t.steer, -1.f, 1.f);
    input.throttle = std::max(input.throttle, t.throttle);
    input.brake = std::max(input.brake, t.brake);
    input.nitro = input.nitro || t.nitro;
    input.horn = input.horn || t.horn;
    input.handbrake = input.handbrake || t.handbrake;
    input.pause = input.pause || t.pause;
}

void Game::pause() {
    if (paused_) return;
    touch_.release();
    paused_ = true;
    menu_.open(!web);
    SynthParams quiet = sound_;
    quiet.volume = 0.f;
    synth_.set_params(quiet);
}

bool Game::update_pause(const InputState& input) {
    if (!paused_) {
        // In a web page Esc pauses: there is nothing to quit to.
        if (input.escape && !web) return false;
        if (input.restart) reset();
        if (input.pause || (input.escape && web)) pause();
        return true;
    }
    if (options_open_ && !input.pause) {
        const Options before = options_;
        OptionsAction act = options_menu_.update(input.menu, options_);
        for (const Finger& tap : touch_taps_) {
            if (act != OptionsAction::None) break;
            float x = 0.f, y = 0.f;
            display_->screen_to_framebuffer(tap.x, tap.y, x, y);
            const MenuTap where = options_tap(x, y, width_, fb_height(), OptionsMenu::items);
            act = options_menu_.choose(where.item, where.side, options_);
        }
        apply_options(before);
        if (options_.time != before.time || options_.fuel != before.fuel || options_.nitros != before.nitros ||
            options_.police != before.police || options_.weather != before.weather ||
            options_.traffic != before.traffic || act == OptionsAction::Back)
            save_choices();
        if (act == OptionsAction::Back) {
            options_open_ = false;
        } else if (act == OptionsAction::StartZone) {
            options_open_ = false;
            paused_ = false;
            reset();
            start_at(zone_start_position(options_menu_.zone));
        } else if (act == OptionsAction::ChangeTrack) {
            options_open_ = false;
            paused_ = false;
            if (options_menu_.track != track_index_) {
                load_track(options_menu_.track);
                save_choices();
            } else {
                reset();
            }
        }
        return true;
    }
    if (debug_open_ && !input.pause) {
        const float hour_before = hour_;
        const int car_before = car_model_;
        bool close = debug_menu_.update(input.menu, debug_, hour_, car_model_, driver_, passenger_);
        for (const Finger& tap : touch_taps_) {
            float x = 0.f, y = 0.f;
            display_->screen_to_framebuffer(tap.x, tap.y, x, y);
            const MenuTap where = options_tap(x, y, width_, fb_height(), DebugMenu::items, 8, 28, 14);
            close = debug_menu_.choose(where.item, where.side, debug_, hour_, car_model_, driver_, passenger_) ||
                    close;
        }
        if (hour_ != hour_before) {
            // Free the clock from a fixed Day/Dusk/Night setting so the scrub sticks.
            options_.time = TimeSetting::Cycle;
        }
        if (car_model_ != car_before) apply_car();
        if (close) {
            if (debug_menu_.selected == DebugMenu::Sprites)
                run_sprite_viewer_session(*display_, input_, sprites_);
            if (debug_menu_.selected == DebugMenu::Attract) {
                debug_open_ = false;
                paused_ = false;
                start_attract(true);
                return true;
            }
            debug_open_ = false;
        }
        return true;
    }
    if (video_open_ && !input.pause) {
        bool want_fs = false;
        bool hd = pixel_scale_ >= 2;
        PresentBackend present = present_backend_;
        bool close = video_menu_.update(input.menu, wide_, hd, want_fs, present, debug_);
        for (const Finger& tap : touch_taps_) {
            float x = 0.f, y = 0.f;
            display_->screen_to_framebuffer(tap.x, tap.y, x, y);
            const MenuTap where = options_tap(x, y, width_, fb_height(), VideoMenu::items, 12, 36, 16);
            close = video_menu_.choose(where.item, where.side, wide_, hd, want_fs, present, debug_) || close;
        }
        set_pixel_scale(hd ? 2 : 1);
        if (want_fs) display_->toggle_fullscreen();
        if (present != present_backend_) {
            if (display_->set_present_backend(present)) {
                present_backend_ = present;
                gles_.invalidate();
                apply_scene_backend();
            } else present_backend_ = display_->present_backend() == PresentBackend::Gl ? PresentBackend::Gl
                                   : PresentBackend::Sdl;
        }
        save_choices();
        if (close) video_open_ = false;
        return true;
    }
    if (audio_open_ && !input.pause) {
        const int music_before = music_;
        const int eng_before = engine_vol_, mus_before = music_vol_;
        const bool muted_before = muted_;
        bool close = audio_menu_.update(input.menu, muted_, engine_vol_, music_vol_, music_);
        for (const Finger& tap : touch_taps_) {
            float x = 0.f, y = 0.f;
            display_->screen_to_framebuffer(tap.x, tap.y, x, y);
            const MenuTap where = options_tap(x, y, width_, fb_height(), AudioMenu::items);
            close = audio_menu_.choose(where.item, where.side, muted_, engine_vol_, music_vol_, music_) || close;
        }
        if (music_ != music_before) synth_.set_music(music_);
        if (music_ != music_before || eng_before != engine_vol_ || mus_before != music_vol_ || muted_ != muted_before)
            save_choices();
        if (close) audio_open_ = false;
        return true;
    }
    options_open_ = false;
    video_open_ = false;
    audio_open_ = false;
    debug_open_ = false;
    MenuAction action = input.pause ? MenuAction::Resume : menu_.update(input.menu);
    for (const Finger& tap : touch_taps_) {
        if (action != MenuAction::None) break;
        float x = 0.f, y = 0.f;
        display_->screen_to_framebuffer(tap.x, tap.y, x, y);
        const MenuTap where = menu_tap(menu_, x, y, width_, fb_height());
        action = menu_.choose(where.item, where.side);
    }
    switch (action) {
        case MenuAction::None: return true;
        case MenuAction::Quit: return false;
        case MenuAction::Restart: reset(); break;
        case MenuAction::Options:
            options_open_ = true;
            options_menu_.open(zone_, static_cast<int>(track_.zones.size()), track_index_, track_count);
            return true;
        case MenuAction::Video:
            video_open_ = true;
            video_menu_.open();
            return true;
        case MenuAction::Audio:
            audio_open_ = true;
            audio_menu_.open();
            return true;
        case MenuAction::Debug:
            debug_open_ = true;
            debug_menu_.open();
            return true;
        case MenuAction::Resume: break;
    }
    paused_ = false;
    return true;
}

void Game::start_at(float position) {
    world_.get<Transform>(player_).z = track_.wrap(position);
    place_on_road(vertical_, track_.height_at(position + world_.get<Camera>(camera_).player_z()));
    zone_ = -1;
}

float Game::zone_start_position(int index) const {
    const Zone& zone = track_.zones.at(static_cast<size_t>(index));
    // Past the fade into the zone, and the camera sits player_z behind the car.
    const float cam = world_.get<Camera>(camera_).player_z();
    return track_.wrap(static_cast<float>(zone.first_segment + 80) * track_.segment_length - cam);
}

void Game::load_track(int index) {
    track_index_ = ((index % track_count) + track_count) % track_count;
    track_ = build_track(track_index_);
    for (int k = 0; k < lot_kinds; ++k) lots_[static_cast<size_t>(k)] = track_.lots(static_cast<Lot>(k));
    map_ = track_map(track_);
    record_lap_ = best_lap(store_.load_laps(), track_name(track_index_));
    reset();
}

std::string Game::zone_label(int index) const {
    const Zone& zone = track_.zones.at(static_cast<size_t>(index));
    const auto same = std::count_if(track_.zones.begin(), track_.zones.end(),
                                    [&](const Zone& z) { return z.country == zone.country; });
    return same > 1 && track_index_ != 0 ? zone.country + " " + zone.region : zone.country;
}

void Game::print_zones() const {
    for (size_t i = 0; i < track_.zones.size(); ++i) {
        const Zone& z = track_.zones[i];
        const int end = i + 1 < track_.zones.size() ? track_.zones[i + 1].first_segment
                                                    : static_cast<int>(track_.segments.size());
        std::printf("%zu\t%s\t%s\tsegments %d..%d\tposition %.0f\n", i, z.country.c_str(), z.region.c_str(),
                    z.first_segment, end - 1, zone_start_position(static_cast<int>(i)));
    }
}

bool Game::screenshot(const ScreenshotOptions& opts) {
    // Headless, the screen is the framebuffer.
    set_width(opts.width);
    wide_ = width_ > fb_base_width();
    touch_.set_layout(TouchLayout{static_cast<float>(width_), static_cast<float>(fb_height()), 0.f, 0.f,
                                  static_cast<float>(width_), static_cast<float>(fb_height())});
    const float start = opts.zone >= 0 ? zone_start_position(opts.zone) : opts.position;
    start_at(start);
    if (opts.fuel >= 0.f) fuel_.set(opts.fuel);
    if (opts.car >= 0) {
        car_model_ = opts.car % car_models;
        apply_car();
    }
    if (opts.visit >= 0) autopilot_visit_ = static_cast<Lot>(opts.visit);
    if (opts.storm >= 0.f) front_.force(opts.storm);
    synth_.set_music(opts.music);
    view_mode_ = static_cast<ViewMode>(((opts.view % view_modes) + view_modes) % view_modes);
    if (opts.dirt >= 0.f) dirt_.set(opts.dirt, opts.dirt);
    std::vector<int16_t> sound;
    constexpr int samples_per_step = Synth::sample_rate / 60; // 735, exactly
    int stopped_at = -1; // --brake: when the car came to a stop
    for (int i = 0; i < opts.frames; ++i) {
        InputState in = autopilot();
        if (opts.force_steer && i >= opts.steer_from) in.steer = opts.steer;
        in.horn = opts.horn;
        in.nitro = i == opts.nitro_frame;
        in.handbrake = opts.handbrake_from >= 0 && i >= opts.handbrake_from;
        if (i == 0 && opts.attract) start_attract();
        if (i == opts.police_frame) start_chase();
        if (i == 0) {
            if (opts.hour >= 0.f) hour_ = opts.hour;
            headlights_ = opts.headlights;
            // The hazards' switch: on a police car its lightbar.
            const bool lightbar = body_has_lightbar(car_model(car_model_).body);
            hazards_ = opts.signal == hazard_signal && !lightbar;
            beacon_ = opts.signal == hazard_signal && lightbar;
            signal_ = opts.signal == hazard_signal ? 0 : opts.signal;
            signal_x_ = world_.get<Transform>(player_).x;
        }
        if (attract_) {
            update_attract(fixed_dt_);
            continue;
        }
        if (!opts.touches.empty()) {
            touch_seen_ = true;
            apply_touch(in, opts.touches);
        }
        // Brake to a stop, let go a moment, then hold it again: reverse.
        if (opts.brake_from >= 0 && i >= opts.brake_from) {
            if (stopped_at < 0 && world_.get<Velocity>(player_).speed <= 0.f) stopped_at = i;
            in.brake = stopped_at >= 0 && i < stopped_at + 10 ? 0.f : 1.f;
            in.throttle = 0.f;
        }
        fixed_update(in, fixed_dt_);
        if (!opts.wav_path.empty()) {
            sound.resize(sound.size() + samples_per_step);
            synth_.render(sound.data() + sound.size() - samples_per_step, samples_per_step);
        }
    }
    if (!opts.wav_path.empty() && !write_wav(opts.wav_path, sound, Synth::sample_rate)) {
        std::cerr << "Writing " << opts.wav_path << " failed\n";
        return false;
    }
    paused_ = opts.pause;
    render();
    draw_touch();
    overlay_.draw(fb_);
    if (opts.pause) {
        menu_.open();
        draw_pause_menu(fb_, menu_, zone_label(zone_), track_name(track_index_));
    }
    return save_bmp(opts.path, fb_.pixels(), width_, fb_height());
}

InputState Game::autopilot() const {
    const auto& tr = world_.get<Transform>(player_);
    const auto& vel = world_.get<Velocity>(player_);
    const auto& player = world_.get<Player>(player_);
    const auto& cam = world_.get<Camera>(camera_);

    const Segment& seg = track_.segment_at(tr.z + cam.player_z());
    const float pct = vel.speed / player.max_speed;
    // Lateral push the curve will apply this tick, relative to one steering tick.
    const float grip = std::max(look_at(tr.z + cam.player_z()).grip * car_model(car_model_).grip, 0.2f);
    const float drift = -pct * seg.curve * player.centrifugal / grip;

    // Low on fuel with a gas station coming up: move over to the right, slow
    // down, pull onto the forecourt and wait there until the tank is full.
    // Asked to visit another kind of lot (--visit), it does the same there, and stays.
    const RoadTheme& side_look = track_.look_at(tr.z + cam.player_z());
    // The lane going its way nearest the middle of the road.
    float target_x = lane_center(side_look.lanes, nearest_own_lane(side_look.lanes, side_look.left_hand, 0.f));
    const float cruise_x = target_x;
    float speed_limit = 1.f;
    const bool fuel = fuel_.level() < 0.45f || (refuelling_ && !fuel_.full());
    if (fuel || autopilot_visit_) {
        const int n = static_cast<int>(track_.segments.size());
        const int here = track_.index_at(tr.z + cam.player_z());
        const Lot visit = fuel ? Lot::Gas : *autopilot_visit_;
        const bool on = seg.forecourt >= forecourt_width && seg.lot == visit;
        for (int start : lots_[static_cast<size_t>(visit)]) {
            const int ahead = ((start - here) % n + n) % n;
            if (!on && ahead > 120) continue;
            const float side = static_cast<float>(track_.segment(start).court_side);
            target_x = side * (on || seg.forecourt > 1.3f ? 1.45f : 0.6f);
            // At the pumps it creeps; at any other lot it stops, for the offer.
            speed_limit = on ? (visit != Lot::Gas && lot_here() == visit ? 0.f : refuel_speed * 0.6f)
                             : 0.1f + 0.9f * static_cast<float>(ahead) / 120.f;
            break;
        }
    }
    const float wanted = (target_x - tr.x) * 4.f - drift;

    InputState in;
    in.steer = wanted > 0.3f ? 1.f : wanted < -0.3f ? -1.f : 0.f;
    const bool coast = std::abs(drift) > 1.f && std::abs(tr.x - cruise_x) > 0.6f && target_x == cruise_x;
    const bool slow = pct > speed_limit || speed_limit <= 0.f; // a limit of 0: stand on the brake
    in.throttle = coast || slow || pct > speed_limit * 0.9f ? 0.f : 1.f;
    in.brake = coast || slow ? 1.f : 0.f;
    return in;
}

void Game::fixed_update(const InputState& driver_input, float dt) {
    // Pulled over by the police, the car stands at the side of the road.
    InputState input = driver_input;
    if (pulled_over_ > 0.f) {
        input.throttle = 0.f;
        input.brake = 1.f;
        input.nitro = false;
        reverse_armed_ = false; // held on the brake, not backing away
    }
    update_particles(dt);
    wheel_distance_ = std::fmod(wheel_distance_ + world_.get<Velocity>(player_).speed * dt,
                                SpriteSheet::tread_step * SpriteSheet::tyre_frames * 1000.f);
    if (crash_time_ >= 0.f) {
        update_crash(dt);
        return;
    }

    auto& tr = world_.get<Transform>(player_);
    auto& vel = world_.get<Velocity>(player_);
    const auto& player = world_.get<Player>(player_);
    const auto& cam = world_.get<Camera>(camera_);
    const float player_z = cam.player_z();

    const Segment& seg = track_.segment_at(tr.z + player_z);
    // Most things go by how fast the car goes, either way; reversing
    // (negative speed) only its movement along the track cares about.
    const float speed_pct = std::abs(vel.speed) / player.max_speed;
    const float dx = dt * steer_rate(speed_pct);

    // Horn, nitro and the wave after a close pass.
    horn_ = input.horn;
    // In a city at night the horn wakes nearby buildings: every window lights.
    {
        const float player_z = world_.get<Camera>(camera_).player_z();
        const float z = world_.get<Transform>(player_).z + player_z;
        const RoadTheme town = track_.look_at(z);
        const Daylight sky = daylight_at(hour_);
        if (horn_ && town.night_glow >= 0.35f && sky.level < 0.5f) window_wake_time_ = 2.f;
        if (window_wake_time_ > 0.f) window_wake_time_ -= dt;
    }
    nitro_.update(dt);
    if (input.nitro && !nitro_held_ && !fuel_.empty() && nitro_.fire() && car_model_ == scanner_model &&
        !vertical_.airborne) {
        // The scanner car's turbo boost: a leap.
        vertical_.vy = scanner_jump;
        vertical_.airborne = true;
    }
    nitro_held_ = input.nitro;
    if (wave_time_ > 0.f) wave_time_ -= dt;

    const float prev_z = tr.z;
    tr.z = track_.wrap(tr.z + dt * vel.speed);
    background_.update(seg.curve, dt * vel.speed / track_.segment_length, dt);

    // Over a crest taken fast the car leaves the road; in the air it can't
    // steer, drive or brake, and it passes over whatever is below it.
    const Segment& under = track_.segment_at(tr.z + player_z);
    const float landing = step_vertical(vertical_, track_.height_at(tr.z + player_z),
                                        (under.y2 - under.y1) / track_.segment_length * vel.speed, jump_gravity, dt);
    const bool airborne = vertical_.airborne;
    if (landing_time_ > 0.f) landing_time_ -= dt;
    view_yaw_ *= std::exp(-dt / view_turn_seconds);
    view_shift_ *= std::exp(-dt / view_turn_seconds);

    // Wet or icy roads give the tyres less to bite on: steering has less
    // effect and the car is pushed further out of curves.
    // Ploughing through a wet spot fast, the tyres lose most of their grip
    // too: the car barely steers, slides out of bends, twitches and slows.
    const RoadTheme look = look_at(tr.z + player_z);
    // On an oil slick there is next to no grip at all: in a bend the car
    // slides out, on a straight it twitches, and it squeals.
    const Patch surface = airborne ? Patch::None
                                 : track_.patch_under(tr.z + player_z, tr.x,
                                                      player.car_width / track_.half_width_at(tr.z + player_z) / 2.f);
    wet_ = surface == Patch::Water && speed_pct > 0.05f;
    aquaplaning_ = wet_ && speed_pct > aquaplane_speed;
    oily_ = surface == Patch::Oil && speed_pct > oil_speed;
    if (oily_) spin_time_ = 0.6f;
    else if (spin_time_ > 0.f) spin_time_ -= dt;
    // The handbrake locks the rear wheels: the tail swings out of bends and
    // the steering bites harder, a handbrake turn.
    handbraking_ = input.handbrake && !airborne && speed_pct > handbrake_speed;
    const float grip = std::max(look.grip * car_model(car_model_).grip, 0.2f) * (aquaplaning_ ? aquaplane_grip : 1.f) *
                       (oily_ ? oil_grip : 1.f) * (handbraking_ ? handbrake_grip : 1.f);
    steer_ = input.steer > 0.3f ? 1 : input.steer < -0.3f ? -1 : 0;
    update_indicators(input, dt);
    wheel_angle_ += (input.steer * 1.4f - wheel_angle_) * std::min(1.f, dt * 12.f); // the wheel follows the hands
    braking_ = input.brake > 0.1f && vel.speed >= 0.f; // reversing, it's the reverse gear
    if (!airborne) {
        tr.x += dx * input.steer * (0.5f + 0.5f * grip) * (handbraking_ ? handbrake_steer : 1.f);
        // The S-bends where a fork's routes part are a split, not a corner:
        // they push the car outwards only a little, so it stays on the road
        // it is on instead of drifting over to the other one.
        const float push = seg.branch_bend ? fork_bend_push : 1.f;
        tr.x -= dx * speed_pct * seg.curve * player.centrifugal * push / grip;
        follow_fork(prev_z + player_z);
    }
    if (oily_) {
        rng_ = rng_ * 1664525u + 1013904223u;
        tr.x += (static_cast<float>(rng_ >> 8) / 16777216.f - 0.5f) * 3.f * speed_pct * dt;
    }
    if (aquaplaning_) {
        rng_ = rng_ * 1664525u + 1013904223u;
        tr.x += (static_cast<float>(rng_ >> 8) / 16777216.f - 0.5f) * 1.6f * speed_pct * dt;
        vel.speed -= player.max_speed * 0.15f * dt;
    }
    if (wet_) {
        spawn_spray(speed_pct);
        dirt_.splash(speed_pct, dt);
    }
    if (surface == Patch::Oil && speed_pct > 0.02f) dirt_.oil(dt);
    dirt_.rinse(look.rain, dt);
    front_.update(dt);
    update_lightning(look.rain, dt);
    if (handbraking_) spawn_smoke(speed_pct);

    weather_.update(look.rain, look.snowfall, -seg.curve * 25.f * speed_pct, speed_pct, dt);

    // Braking overrides the throttle; without either the car coasts down.
    update_fuel(input, dt);
    visit_lot(input);
    update_fares();
    update_movie_cars(input, dt);
    update_wash(dt);
    update_nitro_fill(dt);
    const float drive = engine_on_ ? input.throttle * (1.f - input.brake) : 0.f;
    reverse_armed_ = reverse_armed(reverse_armed_, vel.speed, input.throttle, input.brake);
    const bool reversing = !airborne && engine_on_ && in_reverse(vel.speed, input.throttle, input.brake, reverse_armed_);
    float accel = 0.f;
    if (!airborne) {
        accel = input.handbrake ? 0.f : player.accel * drive;
        if (input.brake > 0.01f) accel += player.brake * input.brake;
        else accel += player.decel * (1.f - drive);
        if (input.handbrake) accel += player.brake * handbrake_decel;
    }
    accel += player.accel * Nitro::thrust * nitro_.intensity();
    const float speed_before = vel.speed;
    if (reversing) {
        vel.speed = reverse_speed(vel.speed, input.throttle, input.brake, player.accel * reverse_accel,
                                  -player.brake, reverse_top * player.max_speed, dt);
    } else {
        vel.speed += accel * dt;
    }

    if (!airborne && std::abs(tr.x) > 1.f) {
        // The forecourt is paved: no slowing down there.
        if (vel.speed > player.offroad_limit && !track_.on_forecourt(tr.z + player_z, tr.x))
            vel.speed += player.offroad_decel * dt;

        // Crash into solid roadside objects on the car's segment.
        const float car_w = player.car_width / track_.half_width_at(tr.z + player_z);
        for (const RoadsideObject& obj : seg.scenery) {
            const SceneryInfo& info = scenery_info(obj.kind);
            if (!info.solid) continue;
            const float w = info.width / track_.half_width_at(tr.z + player_z);
            const float center = info.centered ? obj.offset
                                               : obj.offset + (obj.offset < 0.f ? -w : w) / 2.f;
            if (overlap(tr.x, car_w, center, w)) {
                if (speed_pct >= crash_speed) {
                    start_crash(speed_pct);
                    break;
                }
                // A bump: the car stops dead, set back just short of the
                // segment it hit, so it is clear of the object (and can
                // steer round it) instead of hitting it again next step.
                vel.speed = std::min(vel.speed, 0.f);
                crashed_ = true;
                synth_.trigger_crash(0.55f + 0.45f * speed_pct);
                const float seg_start = static_cast<float>(track_.index_at(tr.z + player_z)) *
                                        track_.segment_length;
                tr.z = track_.wrap(seg_start - 0.25f * track_.segment_length - player_z);
                place_on_road(vertical_, track_.height_at(tr.z + player_z));
                break;
            }
        }
    }

    // Rails and cliffs stop the car; leaning on them scrapes the speed away.
    scraping_ = false;
    const float car_half = player.car_width / track_.half_width_at(tr.z + player_z) / 2.f;
    for (int side = -1; side <= 1; side += 2) {
        const float limit = barrier_limit(seg, side, car_half);
        if (static_cast<float>(side) * tr.x > limit) {
            tr.x = static_cast<float>(side) * limit;
            scraping_ = true;
            scrape_side_ = side;
        }
    }
    if (scraping_) { // scraping slows the car whichever way it goes
        const float drag = player.max_speed * 0.5f * dt;
        vel.speed = vel.speed > 0.f ? std::max(0.f, vel.speed - drag) : std::min(0.f, vel.speed + drag);
    }

    // Rear-ending traffic: bounce off and drop behind it.
    const int car_segment = track_.index_at(tr.z + player_z);
    const float car_w = player.car_width / track_.half_width_at(tr.z + player_z);
    world_.view<Transform, Velocity, Traffic>([&](Entity, Transform& t, Velocity& v, Traffic& traffic) {
        if (airborne || traffic.dir < 0 || vel.speed <= v.speed || track_.index_at(t.z) != car_segment) return;
        const float w = vehicle_info(traffic.kind).width / track_.half_width_at(tr.z + player_z);
        if (!overlap(tr.x, car_w, t.x, w * 0.8f)) return;
        vel.speed = v.speed * (v.speed / vel.speed);
        // A segment behind it: at its own position the two would be drawn
        // on top of each other, which shows once it stands still.
        tr.z = track_.wrap(t.z - player_z - track_.segment_length);
        place_on_road(vertical_, track_.height_at(tr.z + player_z));
        crashed_ = true;
        synth_.trigger_crash(0.4f + 0.4f * speed_pct);
    });

    tr.x = std::clamp(tr.x, -3.f, 3.f);
    const float top = player.max_speed * (nitro_.burning() ? Nitro::top_speed : 1.f);
    if (!reversing) vel.speed = std::max(0.f, limit_speed(speed_before, vel.speed, top, overspeed_drag * player.max_speed, dt));
    tr.y = track_.height_at(tr.z + player_z);
    if (landing > 0.f) land(landing);

    // Engine and road shake; rougher off the road. Whole pixels only, the
    // car is pixel art.
    rng_ = rng_ * 1664525u + 1013904223u;
    const float shake = (std::abs(tr.x) > 1.f ? 2.f : 1.f) * vel.speed / player.max_speed;
    bounce_ = !airborne && (rng_ >> 31) && shake > 0.25f ? -std::round(shake) : 0.f;

    update_laps(prev_z + player_z, tr.z + player_z, dt);
    update_animals(dt);
    update_train(dt);
    update_traffic(dt);
    update_police(dt);
    check_close_passes();
    update_audio(input, dt);
}

// The player's car takes its model's top speed and acceleration (grip is
// applied where the road's grip is).
void Game::save_choices() const {
    Choices c{car_model_, driver_, passenger_ >= motel_passengers ? nobody : passenger_, static_cast<int>(view_mode_), music_, wide_ ? 1 : 0, track_index_, options_,
              -1, -1, -1, engine_vol_, music_vol_, pixel_scale_ >= 2 ? 1 : 0,
              display_ && display_->is_fullscreen() ? 1 : 0, muted_ ? 1 : 0, static_cast<int>(present_backend_)};
    // Where the race is, to go on from there next time; following the
    // traffic in the attract mode, where it was.
    if (!attract_) {
        c.position = static_cast<int>(world_.get<Transform>(player_).z);
        c.minutes = static_cast<int>(hour_ * 60.f);
        c.tank = static_cast<int>(fuel_.level() * 1000.f);
    } else {
        c.position = resume_position_;
        c.minutes = resume_minutes_;
        c.tank = resume_tank_;
    }
    store_.save_choices(c);
}

// Into another car: it comes clean. Fuel stays as it is — a dealer is not a
// free fill-up (gas stations are).
void Game::change_car() {
    apply_car();
    beacon_ = beacon_ && body_has_lightbar(car_model(car_model_).body);
    dirt_.reset();
}

void Game::apply_car() {
    const CarModel& m = car_model(car_model_);
    const Player standard = Player::for_segment_length(track_.segment_length);
    Player& p = world_.get<Player>(player_);
    p.max_speed = standard.max_speed * m.top_speed;
    p.accel = standard.accel * m.acceleration;
    p.car_width = m.width;
}

// Standing on the forecourt of a lot with a choice, it is on offer: at a car
// dealer the cars, at a motel the passengers, at a hospital the drivers.
// Each push of the steering to a side shows the next one that way, and the
// player drives off with whichever is showing. A hospital also patches up
// a driver hurt in a crash.
void Game::visit_lot(const InputState& input) {
    const float speed_pct = std::abs(world_.get<Velocity>(player_).speed) / world_.get<Player>(player_).max_speed;
    const std::optional<Lot> here = speed_pct < refuel_speed ? lot_here() : std::nullopt;
    const bool choice = here == Lot::Dealer || here == Lot::SportsDealer || here == Lot::Motel ||
                        here == Lot::Hospital || here == Lot::Truckstop;
    // On offer only while the car stands, and not as it pulls away: steering
    // out of the lot must not pick another car.
    offer_ = choice && speed_pct < offer_speed && input.throttle < 0.1f ? here : std::nullopt;
    if (here != Lot::Hospital) hospital_ambulance_ = false; // back at a hospital, the drivers first
    // The car the player came in, offered alongside the lot's.
    if (!here) arrived_model_ = -1;
    else if (arrived_model_ < 0) arrived_model_ = car_model_;
    if (here == Lot::Hospital && bandaged_) {
        bandaged_ = false;
        synth_.trigger_ding();
        show_message("PATCHED UP", 2.f);
    }
    const int push = input.steer > 0.5f ? 1 : input.steer < -0.5f ? -1 : 0;
    if (offer_ && push != 0 && lot_steer_ == 0) {
        switch (*offer_) {
            case Lot::Dealer:
            case Lot::SportsDealer:
            case Lot::Truckstop:
                // The lot's range of cars at home in its part of the world, in
                // turn, and the car the player came in.
                car_model_ = next_car_offered(car_model_, arrived_model_, lot_range(*offer_), push,
                                              region_of(track_.zone_at(world_.get<Transform>(player_).z +
                                                                       world_.get<Camera>(camera_).player_z()).country));
                change_car();
                break;
            case Lot::Motel:
                // The motel's people and the empty seat; a fare riding gets out.
                passenger_ = ((passenger_ >= motel_passengers ? nobody : passenger_) + push + motel_passengers) %
                             motel_passengers;
                fare_zone_ = -1;
                break;
            default: {
                // The drivers, and after the last of them the ambulance.
                const int pick = (hospital_ambulance_ ? drivers : driver_) + push;
                const int n = drivers + 1;
                const int next = (pick % n + n) % n;
                const bool was_ambulance = hospital_ambulance_;
                hospital_ambulance_ = next == drivers;
                if (hospital_ambulance_) {
                    car_model_ = ambulance_model;
                    change_car();
                } else {
                    driver_ = next;
                    if (was_ambulance && arrived_model_ >= 0 && car_model_ != arrived_model_) {
                        car_model_ = arrived_model_; // back into the car they came in
                        change_car();
                    }
                }
                break;
            }
        }
        synth_.trigger_ding();
        save_choices();
    }
    lot_steer_ = push;
}

std::optional<Lot> Game::lot_here() const {
    const float car_z = world_.get<Transform>(player_).z + world_.get<Camera>(camera_).player_z();
    const Segment& seg = track_.segment_at(car_z);
    if (seg.forecourt < forecourt_width || !track_.on_forecourt(car_z, world_.get<Transform>(player_).x))
        return std::nullopt;
    return seg.lot;
}

void Game::update_fares() {
    const auto& tr = world_.get<Transform>(player_);
    const float car_z = tr.z + world_.get<Camera>(camera_).player_z();
    const float speed_pct = std::abs(world_.get<Velocity>(player_).speed) / world_.get<Player>(player_).max_speed;
    const bool stopped = speed_pct < fare_stop_speed;
    const int zone = track_.zone_number_at(car_z);

    // A fare riding pays at their zone, wherever the car stops there.
    if (fare_zone_ >= 0 && zone == fare_zone_ && stopped) {
        fare_zone_ = -1;
        passenger_ = nobody;
        ++fares_paid_;
        synth_.trigger_ding();
        show_message("FARE PAID", 2.f);
    }

    if (car_model_ != taxi_model) {
        hails_.clear(); // nobody hails anything else
        return;
    }
    // Forget those passed by, and keep a couple waiting ahead.
    const float seg = track_.segment_length;
    hails_.erase(std::remove_if(hails_.begin(), hails_.end(),
                                [&](const Hail& h) {
                                    const float gap = signed_gap(car_z, h.z, track_.length());
                                    return gap < -hail_behind * seg || gap > 2.f * hail_ahead_max * seg;
                                }),
                 hails_.end());
    while (hails_.size() < hails_waiting) {
        rng_ = rng_ * 1664525u + 1013904223u;
        const float t = static_cast<float>(rng_ >> 8) / 16777216.f;
        const float ahead = hail_ahead_min + (hail_ahead_max - hail_ahead_min) * t +
                            static_cast<float>(hails_.size()) * (hail_ahead_max - hail_ahead_min) / 2.f;
        int i = track_.index_at(car_z + ahead * seg);
        // At the kerb on the side traffic keeps to, clear of forecourts, rails
        // and cliffs.
        for (int n = 0; n < 60; ++n, ++i) {
            const Segment& s = track_.segment(i);
            const bool left = track_.look(i).left_hand;
            if (s.forecourt <= 0.f && !s.tunnel && (left ? s.left : s.right) == Edge::None) break;
        }
        rng_ = rng_ * 1664525u + 1013904223u;
        const int fare = static_cast<int>((rng_ >> 8) % static_cast<uint32_t>(fares));
        const float side = track_.look(i).left_hand ? -1.f : 1.f;
        hails_.push_back({track_.wrap((static_cast<float>(i) + 0.5f) * seg), side * hail_kerb, fare});
    }

    // Stopped beside one: they get in, if the seat is free.
    if (!stopped) return;
    for (auto it = hails_.begin(); it != hails_.end(); ++it) {
        const float gap = signed_gap(car_z, it->z, track_.length());
        if (std::abs(gap) > hail_reach * seg || std::abs(tr.x - it->x) > 1.2f) continue;
        if (passenger_ != nobody) {
            show_message("SEAT TAKEN", 1.f);
            return;
        }
        passenger_ = first_fare + it->fare;
        rng_ = rng_ * 1664525u + 1013904223u;
        const int zones = static_cast<int>(track_.zones.size());
        fare_zone_ = (zone + 1 + static_cast<int>((rng_ >> 8) % 3u)) % zones;
        hails_.erase(it);
        synth_.trigger_ding();
        show_message("TO " + track_.zones[static_cast<size_t>(fare_zone_)].region, 2.5f);
        return;
    }
}

void Game::update_animals(float dt) {
    auto& tr = world_.get<Transform>(player_);
    auto& vel = world_.get<Velocity>(player_);
    const auto& player = world_.get<Player>(player_);
    const float player_z = world_.get<Camera>(camera_).player_z();
    const float car_z = tr.z + player_z;
    const float seg = track_.segment_length;

    // A new one ahead, where the countryside has animals.
    crossing_wait_ -= dt;
    if (crossing_wait_ <= 0.f) {
        rng_ = rng_ * 1664525u + 1013904223u;
        crossing_wait_ = animal_wait(static_cast<float>(rng_ >> 8) / 16777216.f);
        const float z = track_.wrap(car_z + animal_ahead * seg);
        const Animal kind = zone_animal(track_.zone_at(z).decor);
        const Segment& s = track_.segment_at(z);
        if (kind != Animal::None && s.left == Edge::None && s.right == Edge::None && s.forecourt <= 0.f && !s.tunnel) {
            const int dir = (rng_ >> 30) & 1 ? 1 : -1;
            const AnimalInfo& info = animal_info(kind);
            for (int i = 0; i < info.herd; ++i) {
                crossings_.push_back({kind, track_.wrap(z + static_cast<float>(i) * 1.5f * seg),
                                      -static_cast<float>(dir) * (animal_start + 0.35f * static_cast<float>(i)), dir});
            }
        }
    }

    const float car_w = player.car_width / track_.half_width_at(car_z);
    const int car_segment = track_.index_at(car_z);
    const float speed_pct = std::abs(vel.speed) / player.max_speed;
    for (Crossing& a : crossings_) {
        const float gap = signed_gap(car_z, a.z, track_.length());
        // The horn hurries those ahead.
        if (horn_ && gap > 0.f && gap < honk_range * seg) a.hurried = true;
        a.x += static_cast<float>(a.dir) * animal_info(a.kind).speed * (a.hurried ? 3.f : 1.f) * dt;
        // Hit: a bump, or at speed a crash; the animal bolts.
        const float w = animal_info(a.kind).width / track_.half_width_at(car_z);
        if (crash_time_ < 0.f && !vertical_.airborne && !a.hurried && track_.index_at(a.z) == car_segment &&
            overlap(tr.x, car_w, a.x, w)) {
            a.hurried = true;
            if (speed_pct >= crash_speed) {
                start_crash(speed_pct);
            } else {
                vel.speed = std::min(vel.speed, 0.f);
                crashed_ = true;
                synth_.trigger_crash(0.4f + 0.4f * speed_pct);
                tr.z = track_.wrap(static_cast<float>(car_segment) * seg - 0.25f * seg - player_z);
                place_on_road(vertical_, track_.height_at(tr.z + player_z));
            }
        }
    }
    // Gone over the far side, or left behind.
    crossings_.erase(std::remove_if(crossings_.begin(), crossings_.end(),
                                    [&](const Crossing& a) {
                                        return a.x * static_cast<float>(a.dir) > animal_start + 0.5f ||
                                               signed_gap(car_z, a.z, track_.length()) < -animal_behind * seg;
                                    }),
                     crossings_.end());
}

float Game::train_tail() const {
    return train_.front - static_cast<float>(train_.wagons + 1) * train_.car;
}

void Game::update_train(float dt) {
    auto& tr = world_.get<Transform>(player_);
    const auto& vel = world_.get<Velocity>(player_);
    const auto& player = world_.get<Player>(player_);
    const float car_z = tr.z + world_.get<Camera>(camera_).player_z();
    const float seg = track_.segment_length;

    if (!train_.active) {
        // The next crossing ahead, once: a train or not, and when.
        for (int c : track_.crossings()) {
            const float gap = signed_gap(car_z, (static_cast<float>(c) + 0.5f) * seg, track_.length());
            if (c == train_decided_ || gap < train_decide_min * seg || gap > train_decide_max * seg) continue;
            train_decided_ = c;
            rng_ = rng_ * 1664525u + 1013904223u;
            const float roll = static_cast<float>(rng_ >> 8) / 16777216.f;
            if (roll >= train_chance || vel.speed < 0.2f * player.max_speed) break;
            Train t;
            t.segment = c;
            t.dir = (rng_ >> 30) & 1 ? 1 : -1;
            t.wagons = 4 + static_cast<int>((rng_ >> 4) % 5u);
            t.car = train_car_length / track_.half_width(c);
            // At the speed it goes the car gets there in `eta`; the train's
            // tail clears the far side of the road just before.
            const float eta = gap / vel.speed;
            const float length = static_cast<float>(t.wagons + 1) * t.car;
            t.front = 1.3f + length - train_speed * (eta - train_margin);
            t.active = t.front - length < -2.f; // too late to come from out of sight
            train_ = t;
            break;
        }
        return;
    }
    // Still out of sight it keeps its time to the car's: should the car
    // slow down or speed up, the train comes later or sooner; in sight it
    // runs on.
    const float gap = signed_gap(car_z, (static_cast<float>(train_.segment) + 0.5f) * seg, track_.length());
    const float length = static_cast<float>(train_.wagons + 1) * train_.car;
    if (train_.front < -4.f && gap > 0.f && vel.speed > 0.2f * player.max_speed) {
        const float desired = 1.3f + length - train_speed * (gap / vel.speed - train_margin);
        train_.front = std::min(desired, -4.f + train_speed * dt);
    } else {
        train_.front += train_speed * dt;
    }
    if (train_tail() > 12.f) {
        train_.active = false;
        return;
    }
    // On the rails when it comes: a crash.
    if (crash_time_ < 0.f && !vertical_.airborne && track_.index_at(car_z) == train_.segment) {
        const float car_half = player.car_width / track_.half_width_at(car_z) / 2.f;
        const float at = static_cast<float>(train_.dir) * tr.x;
        if (at + car_half > train_tail() && at - car_half < train_.front) start_crash(1.f);
    }
}

void Game::update_movie_cars(const InputState& input, float dt) {
    auto& tr = world_.get<Transform>(player_);
    auto& vel = world_.get<Velocity>(player_);
    const auto& player = world_.get<Player>(player_);
    const float player_z = world_.get<Camera>(camera_).player_z();
    const float car_z = tr.z + player_z;
    const bool horn_pressed = input.horn && !movie_horn_;
    movie_horn_ = input.horn;
    if (crash_time_ >= 0.f) return;
    const auto become = [&](int model, const char* message) {
        if (car_model_ != model) {
            car_model_ = model;
            change_car();
        }
        synth_.trigger_ding();
        show_message(message, 2.5f);
    };

    // Held at 88 mph a while: a flash, sparks, and the clock jumps; the car
    // becomes the time machine.
    const float kmh = std::abs(vel.speed) / base_max_speed_ * top_speed_kmh;
    if (mph88_ < 0.f) mph88_ = std::min(0.f, mph88_ + dt);
    else if (kmh > mph88_low && kmh < mph88_high && !vertical_.airborne) mph88_ += dt;
    else mph88_ = std::max(0.f, mph88_ - 0.5f * dt); // drifting off a moment loses a little
    if (mph88_ > mph88_seconds) {
        mph88_ = -20.f;
        flash_time_ = 0.5f;
        synth_.trigger_whoosh(1.f);
        if (options_.time == TimeSetting::Cycle) {
            rng_ = rng_ * 1664525u + 1013904223u;
            hour_ = advance_hour(hour_, (6.f + 12.f * static_cast<float>(rng_ >> 8) / 16777216.f) / 24.f * day_seconds);
        }
        const float x = static_cast<float>(width_) / 2.f;
        for (int i = 0; i < 30; ++i) {
            rng_ = rng_ * 1664525u + 1013904223u;
            const float a = static_cast<float>(rng_ >> 8) / 16777216.f;
            particles_.push_back({x + (a - 0.5f) * 90.f, (static_cast<float>(fb_height()) - 3.f) - 4.f, (a - 0.5f) * 400.f, -60.f - 200.f * a, 0.7f, 0.7f,
                                  1.f, i % 2 ? Color{0xff, 0xa0, 0x30} : Color{0x80, 0xc0, 0xff}, Particle::Kind::Debris});
        }
        become(time_car_model, "88 MPH!");
    }

    // Flat out through the outback at night: the last of the V8s.
    const bool night = daylight_at(hour_).level < 0.5f;
    if (night && track_.zone_at(car_z).decor == Decor::Outback && vel.speed >= 0.95f * player.max_speed) {
        night_run_ += dt;
    } else {
        night_run_ = 0.f;
    }
    if (night_run_ > night_run_seconds && car_model_ != interceptor_model) {
        night_run_ = 0.f;
        become(interceptor_model, "LAST OF THE V8S");
    }

    // The horn at a sports car dealer orders the special car.
    if (horn_pressed && offer_ == Lot::SportsDealer) become(spy_car_model, "SPECIAL ORDER");

    // The spy car's horn drops oil on the road behind it.
    if (horn_pressed && car_model_ == spy_car_model && vel.speed > 0.1f * player.max_speed) {
        const int at = track_.index_at(car_z) - 4;
        for (int i = 0; i < 4; ++i) {
            Segment& s = track_.segments[static_cast<size_t>(((at - i) % static_cast<int>(track_.segments.size()) +
                                                              static_cast<int>(track_.segments.size())) %
                                                             static_cast<int>(track_.segments.size()))];
            s.patch = Patch::Oil;
            s.patch_x = tr.x;
            s.patch_w = 0.35f;
        }
        show_message("OIL!", 1.f);
    }

    // Up the ramp into the black truck, slowly and straight: out comes the
    // scanner car.
    world_.view<Transform, Velocity, Traffic>([&](Entity, Transform& t, Velocity& v, Traffic& traffic) {
        if (!traffic.ramp || car_model_ == scanner_model) return;
        const float gap = signed_gap(car_z, t.z, track_.length());
        const float closing = vel.speed - v.speed;
        if (gap > 0.f && gap < 1.5f * track_.segment_length && std::abs(tr.x - t.x) < 0.12f && closing > 0.f &&
            closing < 0.15f * player.max_speed) {
            become(scanner_model, "WELCOME BACK");
            tr.z = track_.wrap(t.z - player_z - 6.f * track_.segment_length);
            vel.speed = v.speed;
            place_on_road(vertical_, track_.height_at(tr.z + player_z));
        }
    });
}

void Game::movie_car_extras(Bitmap& car, int turn) const {
    const int h = SpriteSheet::player_headroom;
    const int s = turn;
    switch (car_model_) {
        case scanner_model: {
            // The red light sweeping to and fro below the tail lights.
            paint::rect(car, 20 + s, h + 29, 56, 2, Color{0x30, 0x04, 0x04});
            const float t = std::fmod(clock_ * 1.2f, 2.f);
            const int x = 20 + s + static_cast<int>((t < 1.f ? t : 2.f - t) * 50.f);
            paint::rect(car, x, h + 29, 6, 2, Color{0xff, 0x30, 0x30});
            paint::rect(car, x + 2, h + 29, 2, 1, Color{0xff, 0xf8, 0xf0});
            break;
        }
        case time_car_model: {
            // The coils glowing in the vents, pulsing.
            const bool on = std::fmod(clock_, 0.3f) < 0.15f;
            const Color glow = on ? Color{0x80, 0xc0, 0xff} : Color{0x30, 0x60, 0xa0};
            for (int x : {33, 53}) paint::rect(car, x + s, h + 21, 10, 5, glow);
            break;
        }
        case interceptor_model: {
            // The blower sticking up out of the bonnet, seen over the wing.
            paint::rect(car, 42 + 2 * turn, h - 8, 12, 6, Color{0x90, 0x90, 0x98});
            paint::rect(car, 44 + 2 * turn, h - 11, 8, 3, Color{0x30, 0x30, 0x34});
            break;
        }
        default: break;
    }
}

// Standing on a car wash's forecourt, the car is washed clean: water and
// foam rain down on it until the last of the dirt is gone.
void Game::update_nitro_fill(float dt) {
    const float speed_pct = std::abs(world_.get<Velocity>(player_).speed) / world_.get<Player>(player_).max_speed;
    if (!parked_at(Lot::Chemical) || speed_pct >= refuel_speed || nitro_.canisters() >= nitro_.capacity()) {
        nitro_fill_ = 0.f;
        return;
    }
    // A canister at a time, while the car stands by the plant.
    nitro_fill_ += dt;
    show_message("FILLING NITRO", 0.2f);
    if (nitro_fill_ >= nitro_fill_seconds) {
        nitro_fill_ = 0.f;
        nitro_.add_canister();
        synth_.trigger_ding();
        if (nitro_.canisters() >= nitro_.capacity()) show_message("NITRO FULL", 2.f);
    }
}

void Game::update_wash(float dt) {
    const float speed_pct = std::abs(world_.get<Velocity>(player_).speed) / world_.get<Player>(player_).max_speed;
    const bool was_washing = washing_;
    washing_ = parked_at(Lot::Wash) && speed_pct < refuel_speed && !dirt_.clean();
    if (!washing_) {
        if (was_washing) message_.clear(); // drove off before it was clean
        return;
    }
    dirt_.wash(dt);
    auto random = [this] {
        rng_ = rng_ * 1664525u + 1013904223u;
        return static_cast<float>(rng_ >> 8) / 16777216.f;
    };
    const Color water{0xc8, 0xe0, 0xf4}, foam{0xf8, 0xfc, 0xff};
    for (int i = 0; i < 3; ++i) {
        const float a = random(), b = random();
        particles_.push_back({static_cast<float>(width_) / 2.f + (a - 0.5f) * 100.f, (static_cast<float>(fb_height()) - 3.f) - 50.f - 10.f * b,
                              (b - 0.5f) * 40.f, 40.f * b, 0.5f, 0.5f, 1.f + b, blend(water, foam, b),
                              Particle::Kind::Spray});
    }
    if (random() < 0.3f) {
        const float a = random();
        particles_.push_back({static_cast<float>(width_) / 2.f + (a - 0.5f) * 80.f, (static_cast<float>(fb_height()) - 3.f) - 30.f, 0.f, -8.f, 0.6f,
                              0.6f, 3.f + 3.f * a, foam, Particle::Kind::Dust});
    }
    if (dirt_.clean()) {
        washing_ = false;
        synth_.trigger_ding();
        show_message("SPARKLING CLEAN", 2.f);
    } else {
        show_message("WASHING", 0.2f);
    }
}

// Burns fuel with the engine's load, refuels on a forecourt, sputters when
// nearly empty and dies when empty; stranded, the driver uses a spare can.
void Game::update_fuel(const InputState& input, float dt) {
    const auto& vel = world_.get<Velocity>(player_);
    const auto& player = world_.get<Player>(player_);
    const float speed_pct = std::abs(vel.speed) / player.max_speed;

    const bool at_pump = parked_at(Lot::Gas) && speed_pct < refuel_speed;
    const bool was_refuelling = refuelling_;
    refuelling_ = at_pump && !fuel_.full();
    if (refuelling_) {
        fuel_.refuel(dt);
        stranded_time_ = 0.f;
        if (fuel_.full()) {
            refuelling_ = false;
            synth_.trigger_ding();
            show_message("TANK FULL", 2.f);
        } else {
            show_message("REFUELLING", 0.2f);
        }
    } else if (was_refuelling) {
        message_.clear(); // drove off before the tank was full
    }

    if (options_.fuel) fuel_.burn(Fuel::load(input.throttle * (1.f - input.brake), drivetrain::rpm(speed_pct)), dt);
    // Down to the reserve: a warning, once a tank, with the way to the next
    // gas station, to plan the stop.
    if (fuel_.level() > Fuel::reserve) {
        fuel_warned_ = false;
    } else if (!fuel_warned_ && options_.fuel) {
        fuel_warned_ = true;
        const int n = static_cast<int>(track_.segments.size());
        const int here = track_.index_at(world_.get<Transform>(player_).z + world_.get<Camera>(camera_).player_z());
        int ahead = n;
        for (int start : lots_[static_cast<size_t>(Lot::Gas)]) ahead = std::min(ahead, ((start - here) % n + n) % n);
        const float km = static_cast<float>(ahead) * track_.segment_length / base_max_speed_ * top_speed_kmh / 3600.f;
        char text[32];
        std::snprintf(text, sizeof text, "GAS IN %.1f KM", static_cast<double>(km));
        synth_.trigger_ding();
        show_message(text, 3.f);
    }
    rng_ = rng_ * 1664525u + 1013904223u;
    const bool sputter = fuel_.level() < 0.03f && (rng_ >> 28) < 4; // a quarter of the time
    engine_on_ = !fuel_.empty() && !sputter;

    if (fuel_.empty()) {
        if (message_ != "OUT OF FUEL") show_message("OUT OF FUEL", 1.f);
        message_time_ = 1.f;
        if (vel.speed < 1.f) stranded_time_ += dt;
        if (stranded_time_ > stranded_seconds) {
            fuel_.set(spare_can);
            stranded_time_ = 0.f;
            show_message("SPARE CAN", 2.f);
        }
    }
}

// Hitting something solid at speed: the car tumbles off (crash_pose()) and is
// put back on the road in the lane nearest to where it crashed.
void Game::start_crash(float speed_pct) {
    const auto& tr = world_.get<Transform>(player_);
    crash_time_ = 0.f;
    crash_side_ = tr.x < 0.f ? -1 : 1;
    crash_x_ = tr.x;
    // Back on the road in the outer lane going the player's way on that side.
    const RoadTheme& look = track_.look_at(tr.z + world_.get<Camera>(camera_).player_z());
    crash_target_x_ = lane_center(look.lanes, nearest_own_lane(look.lanes, look.left_hand,
                                                                static_cast<float>(crash_side_) * 2.f));
    crashed_ = true;
    nitro_.stop();
    wave_time_ = 0.f;
    synth_.trigger_crash(1.f);
    dirt_.crash(speed_pct);
    bandaged_ = true;

    // Bits of car flying off, and a cloud of dust.
    const float x = static_cast<float>(width_) / 2.f;
    const Color chips[] = {{0xd0, 0x18, 0x1c}, {0x88, 0x08, 0x10}, {0x9a, 0x9a, 0xa8}, {0x20, 0x20, 0x24}, {0xa0, 0xc8, 0xe8}};
    for (int i = 0; i < 24; ++i) {
        rng_ = rng_ * 1664525u + 1013904223u;
        const float a = static_cast<float>(rng_ >> 8) / 16777216.f;
        rng_ = rng_ * 1664525u + 1013904223u;
        const float b = static_cast<float>(rng_ >> 8) / 16777216.f;
        particles_.push_back({x + (a - 0.5f) * 60.f, (static_cast<float>(fb_height()) - 3.f) - 20.f * b,
                              (a - 0.5f) * 320.f + static_cast<float>(crash_side_) * 60.f, -80.f - 220.f * b,
                              0.9f + 0.6f * b, 0.9f + 0.6f * b, 1.f, chips[i % 5], Particle::Kind::Debris});
    }
    spawn_dust(x, (static_cast<float>(fb_height()) - 3.f), 10, 1.f + speed_pct);
}

void Game::update_crash(float dt) {
    auto& tr = world_.get<Transform>(player_);
    auto& vel = world_.get<Velocity>(player_);
    const auto& player = world_.get<Player>(player_);
    const float player_z = world_.get<Camera>(camera_).player_z();

    const float t0 = crash_time_;
    crash_time_ += dt;
    // The car skids to a stop at the crash site.
    vel.speed = std::max(0.f, vel.speed - player.max_speed * 1.2f * dt);
    const float prev_z = tr.z;
    tr.z = track_.wrap(tr.z + dt * vel.speed);
    const Segment& seg = track_.segment_at(tr.z + player_z);
    background_.update(seg.curve, dt * vel.speed / track_.segment_length, dt);
    const RoadTheme look = look_at(tr.z + player_z);
    weather_.update(look.rain, look.snowfall, 0.f, vel.speed / player.max_speed, dt);
    tr.y = track_.height_at(tr.z + player_z);
    place_on_road(vertical_, tr.y);

    const CrashPose pose = crash_pose(crash_time_, crash_side_);
    tr.x = crash_x_ + (crash_target_x_ - crash_x_) * pose.recover;
    const int landings = crash_landings(t0, crash_time_);
    if (landings > 0) {
        // Each touchdown is softer than the one before.
        const float strength = pose.recover > 0.f ? 0.f : 1.f - std::min(crash_time_ / crash_tumble_seconds, 0.8f);
        synth_.trigger_crash(0.3f + 0.4f * strength);
        spawn_dust(static_cast<float>(width_) / 2.f + pose.slide, (static_cast<float>(fb_height()) - 3.f), 6, 0.5f + strength);
        crashed_ = true;
    }
    scraping_ = false;
    bounce_ = 0.f;
    steer_ = 0;
    braking_ = false;
    wet_ = aquaplaning_ = oily_ = handbraking_ = reverse_armed_ = false;
    spin_time_ = 0.f;
    if (crash_time_ >= crash_seconds) {
        crash_time_ = -1.f;
        vel.speed = 0.f;
    }

    update_laps(prev_z + player_z, tr.z + player_z, dt);
    update_traffic(dt);
    update_police(dt);
    check_close_passes();
    update_audio(InputState{}, dt);
}

// In a heavy storm lightning strikes now and then: a flash, and after a
// moment (the further away, the longer and the softer) its thunder.
void Game::update_lightning(float rain, float dt) {
    if (flash_time_ > 0.f) flash_time_ -= dt;
    if (thunder_delay_ >= 0.f) {
        thunder_delay_ -= dt;
        if (thunder_delay_ < 0.f) synth_.trigger_thunder(thunder_strength_);
    }
    rng_ = rng_ * 1664525u + 1013904223u;
    if (static_cast<float>(rng_ >> 8) / 16777216.f >= lightning_rate(rain) * dt || thunder_delay_ >= 0.f) return;
    rng_ = rng_ * 1664525u + 1013904223u;
    const float distance = static_cast<float>(rng_ >> 8) / 16777216.f; // 0 overhead .. 1 far off
    flash_time_ = 0.08f + 0.1f * (1.f - distance);
    thunder_delay_ = 0.2f + 2.5f * distance;
    thunder_strength_ = 1.f - 0.7f * distance;
}

void Game::spawn_dust(float x, float y, int count, float strength) {
    const RoadTheme look = look_at(world_.get<Transform>(player_).z + world_.get<Camera>(camera_).player_z());
    const Color dust = blend(look.grass[0], Color{0xc8, 0xc0, 0xb0}, 0.6f);
    for (int i = 0; i < count; ++i) {
        rng_ = rng_ * 1664525u + 1013904223u;
        const float a = static_cast<float>(rng_ >> 8) / 16777216.f;
        rng_ = rng_ * 1664525u + 1013904223u;
        const float b = static_cast<float>(rng_ >> 8) / 16777216.f;
        particles_.push_back({x + (a - 0.5f) * 80.f, y - 4.f * b, (a - 0.5f) * 140.f * strength,
                              -20.f - 40.f * b * strength, 0.7f + 0.5f * b, 0.7f + 0.5f * b,
                              3.f + 3.f * b * strength, blend(dust, Color{0xff, 0xff, 0xff}, 0.3f * b),
                              Particle::Kind::Dust});
    }
}

// Where a fork's routes part, the car is on whichever road it is nearer to:
// crossing over makes that route the active one (the car's lateral position
// is then measured from it). That is possible only while the roads still
// overlap; past that the grass is in between. As the camera follows the road
// it is on, it would snap round to the new road's heading: instead the view
// keeps the old heading and turns over smoothly (view_yaw_). The chosen
// route is announced as the roads separate.
void Game::follow_fork(float prev_car_z) {
    auto& tr = world_.get<Transform>(player_);
    const float car_z = tr.z + world_.get<Camera>(camera_).player_z();
    const int seg = track_.index_at(car_z);
    const int index = track_.branch_at(seg);
    if (index < 0) return;
    const Branch& br = track_.branches[static_cast<size_t>(index)];
    if (seg >= br.fork + br.bend) return;
    const float t = std::fmod(car_z, track_.segment_length) / track_.segment_length;
    // The other road's offset (half-widths) and heading where the camera is,
    // which the view is measured from; a few segments behind the car.
    auto at = [&](float z, bool slope) {
        const int i = track_.index_at(z);
        const float f = std::fmod(track_.wrap(z), track_.segment_length) / track_.segment_length;
        const float a = slope ? track_.branch_slope(i) : track_.branch_offset(i);
        const float b = slope ? track_.branch_slope(i + 1) : track_.branch_offset(i + 1);
        return std::isnan(a) || std::isnan(b) ? 0.f : a + (b - a) * f;
    };
    const float other = track_.branch_offset(seg) + (track_.branch_offset(seg + 1) - track_.branch_offset(seg)) * t;
    if (!std::isnan(other) && std::abs(other) < fork_overlap && nearer_other_road(tr.x, other)) {
        // Keep the camera where it stands and the way it looks, then let it
        // move over: relative to the new road it stands `other - at camera`
        // further out than the car's new position says.
        view_yaw_ += at(tr.z, true);
        view_shift_ += (other - at(tr.z, false)) * track_.half_width_at(tr.z);
        track_.choose_branch(static_cast<size_t>(index), 1 - br.active);
        tr.x -= other;
        map_ = track_map(track_);
    }
    const int announce = br.fork + br.bend / 2;
    if (track_.index_at(prev_car_z) < announce && seg >= announce) show_message(br.names[br.active], 2.f);
}

// Touching down after a jump: the harder, the bigger the thump, the dust and
// the loss of speed; a hard one squashes the car down for a moment.
void Game::land(float impact) {
    const float strength = std::clamp(impact / 8000.f, 0.f, 1.f);
    if (strength < 0.1f) return;
    synth_.trigger_crash(0.2f + 0.5f * strength);
    spawn_dust(static_cast<float>(width_) / 2.f, (static_cast<float>(fb_height()) - 3.f), 3 + static_cast<int>(6.f * strength), 0.4f + strength);
    if (strength > 0.4f) {
        crashed_ = true; // a strong rumble
        landing_time_ = 0.2f;
    }
    world_.get<Velocity>(player_).speed *= 1.f - 0.08f * strength;
}

// Tyre smoke from the locked rear wheels.
void Game::spawn_smoke(float speed_pct) {
    for (int side = -1; side <= 1; side += 2) {
        rng_ = rng_ * 1664525u + 1013904223u;
        const float a = static_cast<float>(rng_ >> 8) / 16777216.f;
        particles_.push_back({static_cast<float>(width_) / 2.f + static_cast<float>(side) * (36.f + 8.f * a), (static_cast<float>(fb_height()) - 3.f) - 3.f,
                              static_cast<float>(side) * 20.f * speed_pct, -15.f - 20.f * a, 0.6f, 0.6f, 2.f + 2.f * a,
                              Color{0xd8, 0xd8, 0xdc}, Particle::Kind::Dust});
    }
}

// Water thrown up by the rear tyres: drops fanning out from their outer
// edges, and a fine mist hanging behind the car.
void Game::spawn_spray(float speed_pct) {
    auto random = [this] {
        rng_ = rng_ * 1664525u + 1013904223u;
        return static_cast<float>(rng_ >> 8) / 16777216.f;
    };
    const Color water{0xc8, 0xd8, 0xe8};
    for (int side = -1; side <= 1; side += 2) {
        const float sd = static_cast<float>(side);
        for (int i = 0; i < 4; ++i) {
            const float a = random(), b = random();
            const float x = static_cast<float>(width_) / 2.f + sd * (40.f + 6.f * a);
            particles_.push_back({x, (static_cast<float>(fb_height()) - 3.f) - 2.f, sd * (40.f + 160.f * a) * speed_pct, -(80.f + 200.f * b) * speed_pct,
                                  0.3f + 0.3f * b, 0.3f + 0.3f * b, 1.f + b, blend(water, Color{0xff, 0xff, 0xff}, b),
                                  Particle::Kind::Spray});
        }
        if (random() < 0.5f) {
            const float a = random();
            particles_.push_back({static_cast<float>(width_) / 2.f + sd * (30.f + 14.f * a), (static_cast<float>(fb_height()) - 3.f) - 4.f,
                                  sd * 30.f * speed_pct, -25.f * speed_pct, 0.45f, 0.45f, 3.f + 3.f * a,
                                  blend(water, Color{0xff, 0xff, 0xff}, 0.5f), Particle::Kind::Dust});
        }
    }
}

void Game::update_particles(float dt) {
    for (Particle& p : particles_) {
        p.life -= dt;
        p.x += p.vx * dt;
        p.y += p.vy * dt;
        if (p.kind == Particle::Kind::Spray) {
            p.vy += 500.f * dt;
            if (p.y > (static_cast<float>(fb_height()) - 3.f) + 4.f) p.life = 0.f; // back on the road
        } else if (p.kind == Particle::Kind::Dust) {
            p.vx *= 1.f - 2.f * dt; // dust hangs in the air and spreads
            p.vy *= 1.f - 2.f * dt;
            p.size += 10.f * dt;
        } else {
            p.vy += 600.f * dt; // debris falls
            if (p.y > (static_cast<float>(fb_height()) - 3.f)) {
                p.y = (static_cast<float>(fb_height()) - 3.f);
                p.vy *= -0.3f;
                p.vx *= 0.6f;
            }
        }
    }
    particles_.erase(std::remove_if(particles_.begin(), particles_.end(),
                                    [](const Particle& p) { return p.life <= 0.f; }),
                     particles_.end());
}

// Passing close by a car: a wave from the side it was passed on, a whoosh
// and a little boost.
void Game::check_close_passes() {
    auto& tr = world_.get<Transform>(player_);
    auto& vel = world_.get<Velocity>(player_);
    const auto& player = world_.get<Player>(player_);
    const float car_z = tr.z + world_.get<Camera>(camera_).player_z();
    const float car_w = player.car_width / track_.half_width_at(car_z);
    const float max_step = 4.f * track_.segment_length;
    world_.view<Transform, Traffic>([&](Entity, Transform& t, Traffic& traffic) {
        const float gap = signed_gap(car_z, t.z, track_.length());
        if (traffic.dir < 0) {
            // Oncoming: met head-on, a crash; close by, only the whoosh.
            const float w = vehicle_info(traffic.kind).width / track_.half_width_at(car_z);
            const bool met = traffic.gap > 0.f && gap <= 0.f && traffic.gap - gap < 2.f * max_step;
            if (met && crash_time_ < 0.f && !vertical_.airborne && overlap(tr.x, car_w, t.x, w * 0.8f)) {
                start_crash(std::min(1.f, std::abs(vel.speed) / player.max_speed + 0.5f));
            } else if (crash_time_ < 0.f && close_pass(traffic.gap, gap, t.x - tr.x, car_w, 2.f * max_step)) {
                synth_.trigger_whoosh(0.3f + 0.5f * std::min(vel.speed / player.max_speed, 1.f));
            }
            traffic.gap = gap;
            return;
        }
        if (crash_time_ < 0.f && close_pass(traffic.gap, gap, t.x - tr.x, car_w, max_step)) {
            const float speed_pct = vel.speed / player.max_speed;
            vel.speed = boosted_speed(vel.speed, player.max_speed);
            wave_side_ = t.x < tr.x ? -1 : 1;
            wave_time_ = 1.2f;
            passed_ = true;
            synth_.trigger_whoosh(0.4f + 0.6f * std::min(speed_pct, 1.f));
        }
        traffic.gap = gap;
    });
}

// Derives the engine and tyre sounds from the state of the car.
void Game::update_audio(const InputState& input, float dt) {
    const auto& tr = world_.get<Transform>(player_);
    const auto& vel = world_.get<Velocity>(player_);
    const auto& player = world_.get<Player>(player_);
    const float player_z = world_.get<Camera>(camera_).player_z();
    const Segment& seg = track_.segment_at(tr.z + player_z);
    const RoadTheme look = look_at(tr.z + player_z);
    const float speed_pct = std::abs(vel.speed) / player.max_speed;

    // The automatic gearbox lifts the throttle briefly on every upshift.
    const int gear = drivetrain::gear(speed_pct);
    if (gear > gear_) shift_cut_ = 0.12f;
    gear_ = gear;
    if (shift_cut_ > 0.f) shift_cut_ -= dt;

    SynthParams p;
    p.rpm = drivetrain::rpm(speed_pct);
    p.throttle = shift_cut_ > 0.f || !engine_on_ ? 0.f : input.throttle * (1.f - input.brake);
    p.engine = engine_on_ ? 1.f : 0.f;
    p.pump = refuelling_ ? 1.f : 0.f;
    p.siren = std::max(siren_, beacon_ ? 0.7f : 0.f);
    p.splash = washing_ ? 0.8f : wet_ ? std::clamp(speed_pct * 1.3f, 0.f, 1.f) : 0.f;
    if (vertical_.airborne) p.rpm = std::min(1.f, p.rpm + 0.25f * input.throttle); // wheels spinning free
    p.speed = speed_pct;

    // The tyres squeal when the lateral demand (steering plus the push of the
    // bend) gets close to what the road's grip allows, and under hard braking.
    const float grip = std::max(look.grip * car_model(car_model_).grip, 0.2f);
    const float slip = (std::abs(input.steer) * speed_pct +
                        std::abs(seg.curve) * player.centrifugal * speed_pct * speed_pct / grip) /
                       (grip * 1.6f);
    float skid = std::clamp((slip - 0.6f) / 0.4f, 0.f, 1.f);
    skid = std::max(skid, 0.7f * std::clamp((input.brake * speed_pct - 0.6f) / 0.4f, 0.f, 1.f));
    const bool off_road = std::abs(tr.x) > 1.f && !track_.on_forecourt(tr.z + player_z, tr.x);
    p.skid = skid * std::clamp(speed_pct * 5.f, 0.f, 1.f) * (off_road ? 0.3f : 1.f);
    if (oily_) p.skid = 1.f;
    if (handbraking_) p.skid = std::max(p.skid, 0.85f);

    p.gravel = off_road && speed_pct > 0.01f ? std::clamp((std::abs(tr.x) - 1.f) * 8.f, 0.f, 1.f) : 0.f;
    p.scrape = scraping_ && vel.speed > 0.f ? 1.f : 0.f;
    p.rain = look.rain;
    p.horn = horn_ ? 1.f : 0.f;
    p.nitro = nitro_.intensity();
    p.throttle = std::max(p.throttle, p.nitro);
    p.volume = muted_ ? 0.f : 1.f;
    p.engine_volume = static_cast<float>(engine_vol_) / static_cast<float>(max_volume);
    p.music_volume = static_cast<float>(music_vol_) / static_cast<float>(max_volume);
    sound_ = p;
    synth_.set_params(p);
}

// Indicator a car shows while changing lanes: -1, +1, or 0 between blinks.
int Game::indicator(Entity e, const Transform& t, const Traffic& traffic) const {
    // The police car's lightbar flashes red, blue, red, ... until it gives up.
    if (traffic.kind == Vehicle::Police) return chase_.giving_up ? 0 : static_cast<int>(clock_ / 0.12f) % 2 ? 1 : -1;
    // An ambulance's lightbar: every other one is on a call.
    if (traffic.kind == Vehicle::Ambulance) return e % 2 ? 0 : static_cast<int>(clock_ / 0.12f) % 2 ? 1 : -1;
    const float d = traffic.target_x - t.x;
    if (traffic.dir < 0 || std::abs(d) < 0.02f) return 0;
    const float phase = static_cast<float>(e % 5) * 0.11f; // they don't all blink in step
    return std::fmod(clock_ + phase, 0.7f) < 0.4f ? (d < 0.f ? -1 : 1) : 0;
}

void Game::update_traffic(float dt) {
    struct Mover { Entity e; float x, z, speed; int dir; };
    std::vector<Mover> movers;
    const auto& ptr = world_.get<Transform>(player_);
    const float player_world_z = ptr.z + world_.get<Camera>(camera_).player_z();
    // (In the attract mode the player's car is where the followed car is:
    // not in anybody's way.)
    if (!attract_) movers.push_back({player_, ptr.x, player_world_z, world_.get<Velocity>(player_).speed, 1});
    // A train on the crossing stands in everyone's way; so do animals.
    if (train_.active && train_.front > -1.5f && train_tail() < 1.5f) {
        const float z = (static_cast<float>(train_.segment) + 0.5f) * track_.segment_length;
        for (float x : {-0.67f, 0.f, 0.67f}) movers.push_back({INVALID_ENTITY, x, z, 0.f, 0});
    }
    for (const Crossing& a : crossings_) {
        if (std::abs(a.x) < 1.2f) movers.push_back({INVALID_ENTITY, a.x, a.z, 0.f, 0});
    }
    world_.view<Transform, Velocity, Traffic>([&](Entity e, Transform& t, Velocity& v, Traffic& traffic) {
        movers.push_back({e, t.x, t.z, v.speed, traffic.dir});
    });

    // In order along the track, so each car looks only at its neighbours:
    // with hundreds of cars on a long track, all against all is too slow.
    std::sort(movers.begin(), movers.end(), [](const Mover& a, const Mover& b) { return a.z < b.z; });
    // Calls fn for every mover from `behind` back to `ahead` along the
    // track around z (wrapping round the lap), and stops when fn says so.
    auto nearby = [&](float z, float behind, float ahead, auto&& fn) {
        const float from = track_.wrap(z - behind);
        size_t i = static_cast<size_t>(std::lower_bound(movers.begin(), movers.end(), from,
                                                        [](const Mover& m, float v) { return m.z < v; }) -
                                       movers.begin());
        for (size_t n = 0; n < movers.size(); ++n, ++i) {
            const Mover& m = movers[i % movers.size()];
            if (track_.wrap(m.z - from) > behind + ahead) break;
            if (!fn(m)) break;
        }
    };

    const float look_ahead = 6.f * track_.segment_length;
    const float follow_ahead = follow_range * track_.segment_length;
    // How far `to` is ahead of `from` for someone going `dir`.
    auto distance_ahead = [&](float from, float to, int dir = 1) {
        return track_.wrap((to - from) * static_cast<float>(dir));
    };
    // Is anyone going the player's way within `range` ahead or half of it
    // behind near lateral position x?
    auto lane_busy = [&](Entity self, float z, float x, float range) {
        bool busy = false;
        nearby(z, range * 0.5f, range, [&](const Mover& m) {
            busy = m.e != self && m.dir >= 0 && std::abs(m.x - x) <= 0.4f;
            return !busy;
        });
        return busy;
    };

    const float max_speed = base_max_speed_; // traffic goes by the standard car, whatever the player drives
    world_.view<Transform, Velocity, Traffic>([&](Entity e, Transform& t, Velocity& v, Traffic& traffic) {
        if (traffic.kind == Vehicle::Police) return; // it chases, see update_police()
        // The road may have a different number of lanes here than where the
        // car was heading, or keep to the other side: aim for the nearest
        // lane that exists and goes its way.
        const RoadTheme& look = track_.look_at(t.z);
        const int lanes = look.lanes;
        const int against = oncoming_lane(lanes, look.left_hand);
        const float spacing = 2.f / static_cast<float>(lanes);
        const int dir = traffic.dir;
        if (dir < 0) {
            // Coming the other way. Meeting the player in its lane, it
            // swerves out and slows down.
            traffic.target_x = lane_center(lanes, against);
            const float towards = distance_ahead(t.z, player_world_z, -1);
            bool alarm = !attract_ && towards < oncoming_alarm * track_.segment_length &&
                         std::abs(ptr.x - t.x) < 0.6f;
            // So does an overtaker heading at it.
            nearby(t.z, oncoming_alarm * track_.segment_length, 0.f, [&](const Mover& m) {
                if (m.dir > 0 && m.e != player_ && std::abs(m.x - t.x) < 0.5f) alarm = true;
                return !alarm;
            });
            if (alarm) traffic.target_x += (look.left_hand ? 1.f : -1.f) * oncoming_dodge;
            float wanted = alarm ? 0.6f * traffic.cruise : traffic.cruise;
            nearby(t.z, follow_ahead, 0.f, [&](const Mover& m) {
                if (m.e == e || m.dir > 0 || std::abs(m.x - t.x) > follow_width) return true;
                const float d = distance_ahead(t.z, m.z, -1);
                if (d < follow_ahead) {
                    wanted = std::min(wanted, follow_speed(traffic.cruise, m.speed, d,
                                                           follow_gap * track_.segment_length, follow_closing));
                }
                return true;
            });
            traffic.braking = v.speed - wanted > 0.02f * max_speed;
            if (wanted < v.speed) v.speed = std::max(wanted, v.speed - traffic_brake * max_speed * dt);
            else v.speed = std::min(wanted, v.speed + traffic_accel * max_speed * dt);
            const float step = (alarm ? 1.4f : 0.8f) * dt;
            t.x += std::clamp(traffic.target_x - t.x, -step, step);
            t.z = track_.wrap(t.z - v.speed * dt);
            return;
        }
        // Is nothing coming the other way for `range` ahead? (In any lane:
        // where traffic changes sides, it crosses over.)
        auto oncoming_clear = [&](float range) {
            bool clear = true;
            nearby(t.z, 0.f, range, [&](const Mover& m) {
                clear = m.dir >= 0;
                return clear;
            });
            return clear;
        };
        // Where traffic changes sides the oncoming lane is another one: an
        // overtaker there is suddenly in its own.
        if (traffic.passing && traffic.pass_lane != against) traffic.passing = false;
        const int own = nearest_own_lane(lanes, look.left_hand, traffic.passing ? t.x : traffic.target_x);
        if (traffic.passing) {
            // Back in once well past whatever it overtook, or early with
            // traffic coming.
            const bool past = !lane_busy(e, t.z, lane_center(lanes, own), 2.f * follow_gap * track_.segment_length);
            if (past || !oncoming_clear(overtake_abort * track_.segment_length)) traffic.passing = false;
        }
        traffic.target_x = lane_center(lanes, traffic.passing ? against : own);

        // Honked at from behind while in the player's way: pull over to the
        // nearest free lane out of the player's line, in a hurry.
        // (An emergency vehicle's lightbar and siren do the same.)
        if ((horn_ || beacon_) && distance_ahead(player_world_z, t.z) < honk_range * track_.segment_length &&
            std::abs(t.x - ptr.x) < honk_clearance && std::abs(traffic.target_x - ptr.x) < honk_clearance) {
            std::vector<bool> busy(static_cast<size_t>(lanes));
            for (int i = 0; i < lanes; ++i) {
                busy[static_cast<size_t>(i)] = i == against || lane_busy(e, t.z, lane_center(lanes, i), look_ahead);
            }
            const int lane = yield_lane(lanes, t.x, ptr.x, honk_clearance, busy);
            if (lane >= 0) {
                traffic.target_x = lane_center(lanes, lane);
                traffic.startled = startled_seconds;
            }
        }
        if (traffic.startled > 0.f) traffic.startled -= dt;

        // Held up by something slower than it would like to go, ahead in
        // this lane? (Slower than its cruising speed, not its speed: once it
        // has slowed down behind, it still wants past.) Pull out if the
        // neighbouring lane is clear.
        bool blocked = false;
        float lead_speed = 0.f;
        nearby(t.z, 0.f, follow_ahead, [&](const Mover& m) {
            blocked = m.e != e && m.dir >= 0 && std::abs(m.x - t.x) <= 0.5f &&
                      m.speed < traffic.cruise - overtake_min_gain * max_speed && distance_ahead(t.z, m.z) < follow_ahead;
            lead_speed = m.speed;
            return !blocked;
        });
        if (blocked && !traffic.passing && std::abs(t.x - traffic.target_x) < 0.05f) {
            bool moved = false;
            for (int i = 0; i < lanes && !moved; ++i) {
                if (i == against) continue;
                const float lane = lane_center(lanes, i);
                if (std::abs(lane - t.x) < 0.15f * spacing || std::abs(lane - t.x) > 1.1f * spacing) continue;
                if (!lane_busy(e, t.z, lane, look_ahead)) { traffic.target_x = lane; moved = true; }
            }
            // No lane of its own to pass in: the oncoming one, when nothing
            // comes for as long as the pass takes.
            const float pass = std::min(overtake_top * max_speed, lead_speed + overtake_boost * max_speed);
            const float gain = pass - lead_speed;
            const float clear = overtake_length / gain * (pass + overtake_oncoming * max_speed) + overtake_abort;
            const float lane = lane_center(lanes, against);
            if (!moved && traffic.startled <= 0.f && gain > overtake_min_gain * max_speed &&
                std::abs(lane - t.x) <= 1.1f * spacing && !lane_busy(e, t.z, lane, look_ahead) &&
                oncoming_clear(clear * track_.segment_length)) {
                traffic.passing = true;
                traffic.pass_lane = against;
                traffic.pass_speed = std::max(traffic.cruise, pass);
                traffic.target_x = lane;
            }
        }

        const float step = (traffic.startled > 0.f ? 1.8f : 0.8f) * dt;
        t.x += std::clamp(traffic.target_x - t.x, -step, step);

        // Following: closing in on something slower in its lane (another car,
        // or the player), a car brakes to keep a gap instead of driving
        // through it, and gets back up to its cruising speed once clear.
        // A rival races the player when they come close.
        float wanted = traffic.kind == Vehicle::Rival
                           ? rival_speed(traffic.cruise, max_speed,
                                         signed_gap(t.z, player_world_z, track_.length()) / track_.segment_length)
                           : traffic.passing ? traffic.pass_speed : traffic.cruise;
        nearby(t.z, 0.f, follow_ahead, [&](const Mover& m) {
            if (m.e == e || m.dir < 0 || std::abs(m.x - t.x) > follow_width) return true;
            const float d = distance_ahead(t.z, m.z);
            if (d < follow_ahead) {
                wanted = std::min(wanted, follow_speed(traffic.cruise, m.speed, d,
                                                       follow_gap * track_.segment_length, follow_closing));
            }
            return true;
        });
        // Small trims while following steadily don't light the brake lights.
        traffic.braking = v.speed - wanted > 0.02f * max_speed;
        if (wanted < v.speed) v.speed = std::max(wanted, v.speed - traffic_brake * max_speed * dt);
        else v.speed = std::min(wanted, v.speed + traffic_accel * max_speed * dt);
        t.z = track_.wrap(t.z + v.speed * dt);
    });
}

// A police car turns up behind the car, lights flashing, siren wailing.
void Game::start_chase() {
    const auto& tr = world_.get<Transform>(player_);
    const float car_z = tr.z + world_.get<Camera>(camera_).player_z();
    police_ = world_.create();
    world_.add<Transform>(police_, Transform{tr.x, 0.f, track_.wrap(car_z - chase_start_gap * track_.segment_length)});
    world_.add<Velocity>(police_, Velocity{world_.get<Velocity>(player_).speed});
    Traffic traffic;
    traffic.kind = Vehicle::Police;
    traffic.target_x = tr.x;
    world_.add<Traffic>(police_, traffic);
    chase_ = Chase{};
    show_message("POLICE!", 2.f);
}

void Game::end_chase() {
    if (police_ != INVALID_ENTITY) world_.destroy(police_);
    police_ = INVALID_ENTITY;
    chase_cooldown_ = chase_cooldown;
    siren_ = 0.f;
}

// The chase: the police car closes in on the car and tails it; tailed long
// enough, the car is pulled over, far enough ahead it has escaped.
void Game::update_police(float dt) {
    const auto& tr = world_.get<Transform>(player_);
    const auto& vel = world_.get<Velocity>(player_);
    const float car_z = tr.z + world_.get<Camera>(camera_).player_z();
    if (pulled_over_ > 0.f) {
        pulled_over_ -= dt;
        if (pulled_over_ <= 0.f) show_message("DRIVE ON", 1.5f); // and the police drive off
    }
    if (police_ == INVALID_ENTITY) {
        if (chase_cooldown_ > 0.f) chase_cooldown_ -= dt;
        rng_ = rng_ * 1664525u + 1013904223u;
        const float roll = static_cast<float>(rng_ >> 8) / 16777216.f;
        if (options_.police && chase_cooldown_ <= 0.f && race_started_ && crash_time_ < 0.f &&
            chase_starts(roll, vel.speed / base_max_speed_, dt)) {
            start_chase();
        }
        return;
    }
    auto& t = world_.get<Transform>(police_);
    auto& v = world_.get<Velocity>(police_);
    auto& traffic = world_.get<Traffic>(police_);
    const float top = base_max_speed_;
    float gap = signed_gap(t.z, car_z, track_.length()) / track_.segment_length;
    PoliceMove move = police_move(chase_, v.speed, vel.speed, tr.x, gap, top, dt);
    if (pulled_over_ > 0.f) move = {0.f, t.x}; // waits in front of the stopped car
    traffic.braking = move.speed < v.speed - 0.02f * top;
    v.speed += std::clamp(move.speed - v.speed, -top * dt, 0.6f * top * dt);
    t.z = track_.wrap(t.z + v.speed * dt);
    t.x += std::clamp(move.x - t.x, -1.2f * dt, 1.2f * dt); // pulling out, cutting in
    // Onto oil (the spy car's), the chase is over: it spins out and drops back.
    if (!chase_.giving_up && chase_.phase != ChasePhase::Leaving &&
        track_.patch_under(t.z, t.x, vehicle_info(Vehicle::Police).width / track_.half_width_at(t.z) / 2.f) == Patch::Oil) {
        chase_.phase = ChasePhase::Leaving;
        chase_.giving_up = true;
        v.speed *= 0.3f;
        synth_.trigger_ding();
        show_message("ESCAPED!", 3.f);
    }
    gap = signed_gap(t.z, car_z, track_.length()) / track_.segment_length;
    const bool done = chase_.giving_up || chase_.phase == ChasePhase::Leaving;
    siren_ = done ? 0.f : 1.f / (1.f + std::abs(gap) / 15.f);
    if (chase_.phase == ChasePhase::Leaving && pulled_over_ <= 0.f && std::abs(gap) > police_leave_gap) {
        end_chase(); // driven off out of sight
        return;
    }

    if (pulled_over_ > 0.f) return;
    switch (update_chase(chase_, gap, vel.speed / top, tr.x - t.x, dt)) {
        case ChaseOutcome::Caught:
            pulled_over_ = pulled_over_seconds;
            show_message("PULLED OVER", pulled_over_seconds);
            break;
        case ChaseOutcome::Escaped:
            // It gives up and drops back, out of sight, unless it already is.
            if (std::abs(gap) > police_leave_gap) end_chase();
            synth_.trigger_ding();
            show_message("ESCAPED!", 3.f);
            break;
        case ChaseOutcome::Going: break;
    }
}

void Game::update_laps(float prev_z, float z, float dt) {
    clock_ += dt;
    advance_clock(dt);

    // Entering a new zone: announce the country and region for a few seconds.
    const int zone = track_.zone_number_at(z);
    if (zone != zone_) {
        zone_ = zone;
        banner_time_ = 4.f;
    }
    if (banner_time_ > 0.f) banner_time_ -= dt;

    if (race_started_) lap_time_ += dt;
    if (message_time_ > 0.f) {
        message_time_ -= dt;
        if (message_time_ <= 0.f) message_.clear();
    }

    if (!crossed_line_forward(prev_z, z, track_.start_z, track_.length())) return;

    if (race_started_) {
        nitro_.refill(); // a full set of canisters for every lap
        last_lap_ = lap_time_;
        // A record beats the best lap so far, of this run or an earlier one;
        // the very first lap is merely the first.
        const bool record = best_lap_ > 0.f && lap_time_ < best_lap_;
        if (record || best_lap_ == 0.f) best_lap_ = record_lap_ = lap_time_;
        show_message(record ? "NEW RECORD" : "LAP " + std::to_string(lap_ + 1), 2.5f);
        store_.add_lap({utc_timestamp(), lap_time_, car_model(car_model_).name, driver(driver_).name,
                        passenger(passenger_).name, track_name(track_index_)});
    } else {
        show_message("GO!", 1.5f);
    }
    race_started_ = true;
    lap_time_ = 0.f;
    ++lap_;
}

void Game::render() {
    const auto& tr = world_.get<Transform>(player_);
    const auto& vel = world_.get<Velocity>(player_);
    const auto& player = world_.get<Player>(player_);
    const auto& cam = world_.get<Camera>(camera_);

    const RoadTheme look = look_at(tr.z + cam.player_z());

    // The camera of the chosen view, placed relative to the car; the car's
    // own reference (the chase camera, see Camera) stays where it is.
    const ViewSetup setup = view_setup(view_mode_, cam.height, cam.depth, body_is_tall(car_model(car_model_).body));
    RoadView view;
    // In the air the camera rises with most of the car's height, and the car
    // lifts on screen by the rest, so the road visibly falls away beneath it;
    // from inside the car the camera rises with all of it.
    const float air = vertical_.y - tr.y;
    // Wrapped: behind the start line a camera further back than the chase
    // camera would stand at a negative position.
    view.position = track_.wrap(tr.z + cam.player_z() - setup.distance);
    view.player_x = tr.x;
    view.player_y = tr.y + (setup.car ? camera_air_share : 1.f) * air;
    view.yaw = view_yaw_;
    view.shift = view_shift_;
    view.camera_height = setup.height;
    view.camera_depth = cam.depth;
    view.player_z = setup.distance;
    view.draw_distance = cam.draw_distance;
    view.fog_density = look.fog_density;
    view.fog_air = atmosphere_air(look);
    {
        // Night: distant objects fade into a darker air (aerial perspective).
        const Daylight dl = daylight_at(hour_);
        const float night = (1.f - dl.level) / (1.f - night_level);
        if (night > 0.02f) {
            view.fog_density *= 1.f + 0.65f * night;
            view.fog_air = blend(view.fog_air, Color{0x18, 0x1c, 0x28}, 0.4f * night);
        }
    }
    view.x_scale = fb_x_unit();
    // Nearby buildings light every window while the wake lasts (city night horn).
    if (window_wake_time_ > 0.f && daylight_at(hour_).level < 0.5f)
        view.window_wake = 55.f * track_.segment_length;
    road_sprites_.clear();
    world_.view<Transform, Traffic>([&](Entity e, Transform& t, Traffic& traffic) {
        RoadSprite s;
        s.z = t.z;
        // Going the player's way it shows its back, coming the other way its
        // front and headlights.
        s.bitmap = traffic.ramp     ? &sprites_.ramp_truck(SpriteSheet::tyre_frame(t.z))
                   : traffic.dir > 0 ? &sprites_.vehicle(traffic.kind, traffic.style, indicator(e, t, traffic),
                                                         traffic.braking, SpriteSheet::tyre_frame(t.z))
                                     : &sprites_.vehicle_front(traffic.kind, traffic.style, 0, SpriteSheet::tyre_frame(t.z));
        s.offset = t.x;
        s.world_width = vehicle_info(traffic.kind).width;
        s.lights = traffic.dir; // their headlights and tail lights at night
        road_sprites_.push_back(s);
    });

    // The train, and the crossing's lamps flashing while it comes.
    if (train_.active) {
        const float z = (static_cast<float>(train_.segment) + 0.5f) * track_.segment_length;
        for (int i = 0; i <= train_.wagons; ++i) {
            const float along = train_.front - (static_cast<float>(i) + 0.5f) * train_.car;
            if (std::abs(along) > 14.f) continue;
            RoadSprite s;
            s.z = z;
            s.bitmap = &sprites_.train_car(i == 0 ? 0 : 1 + (i + train_.segment) % train_wagon_kinds);
            s.offset = static_cast<float>(train_.dir) * along;
            s.world_width = train_car_length;
            s.flip = train_.dir < 0;
            road_sprites_.push_back(s);
        }
        const int lit = static_cast<int>(clock_ / 0.4f) % 2 ? 1 : -1;
        const int sign = train_.segment - 3;
        const float w = scenery_info(Scenery::CrossingSign).width / track_.half_width(sign);
        for (float side : {-1.f, 1.f}) {
            RoadSprite s;
            s.z = static_cast<float>(sign) * track_.segment_length;
            s.bitmap = &sprites_.crossing_sign(lit);
            s.offset = side * (1.15f + w / 2.f);
            s.world_width = scenery_info(Scenery::CrossingSign).width;
            road_sprites_.push_back(s);
        }
    }

    // Animals crossing, facing the way they go.
    for (const Crossing& a : crossings_) {
        RoadSprite s;
        s.z = a.z;
        s.bitmap = &sprites_.animal(a.kind, static_cast<int>(clock_ / (a.hurried ? 0.12f : 0.3f)) & 1);
        s.offset = a.x;
        s.world_width = animal_info(a.kind).width;
        s.flip = a.dir < 0;
        road_sprites_.push_back(s);
    }

    // People hailing the taxi, waving.
    for (const Hail& h : hails_) {
        RoadSprite s;
        s.z = h.z;
        s.bitmap = &sprites_.pedestrian(h.fare, static_cast<int>(clock_ / 0.3f) & 1);
        s.offset = h.x;
        s.world_width = static_cast<float>(s.bitmap->w) * 6.25f;
        road_sprites_.push_back(s);
    }

    // The player's car sits centred, its tyres on the bottom screen row. It
    // is drawn at the projection scale of player_z, which maps car_width to
    // the sprite's native size, so the pixel art is shown 1:1.
    // Twitching after an oil slick, the car flicks from one side to the other.
    const int shown_steer = spin_time_ > 0.f ? (static_cast<int>(clock_ * 16.f) % 2 ? 1 : -1) : steer_;
    // The car with its people drawn over it.
    const Bitmap& body = sprites_.player(car_model_, shown_steer, braking_, SpriteSheet::tyre_frame(wheel_distance_),
                                         shown_signal());
    const Bitmap& people = sprites_.occupants(driver_, passenger_, shown_steer, wave_time_ > 0.f ? wave_side_ : 0,
                                              static_cast<int>(clock_ / 0.15f) & 1, car_model_, bandaged_);
    player_bitmap_ = body;
    apply_dirt(player_bitmap_, dirt_.mud(), dirt_.oil());
    // Seen over the seats or through the rear window; vans, box trucks and
    // the racer show nobody from behind.
    if (body_shows_people(car_model(car_model_).body)) {
        for (size_t i = 0; i < people.px.size() && i < player_bitmap_.px.size(); ++i) {
            if (people.px[i] >> 24) player_bitmap_.px[i] = people.px[i];
        }
    }
    movie_car_extras(player_bitmap_, shown_steer);
    if (beacon_) {
        // The lightbar flashing, red and blue in turn.
        const int lit = static_cast<int>(clock_ / 0.12f) % 2 ? 1 : -1;
        if (car_model(car_model_).body == Body::Ambulance) {
            paint_lightbar(player_bitmap_, lit, ambulance_lightbar_x, ambulance_lightbar_y);
        } else {
            paint_lightbar(player_bitmap_, lit, 48 + 2 * shown_steer, SpriteSheet::player_headroom - 1);
        }
    }
    const Bitmap& car = player_bitmap_;
    const float scale = setup.car ? cam.depth / setup.distance * fb_x_unit() : 1.f;
    RoadSprite me;
    me.z = tr.z + cam.player_z();
    me.bitmap = &car;
    me.fixed = true;
    me.sw = player.car_width * scale;
    me.sh = me.sw * static_cast<float>(car.h) / static_cast<float>(car.w);
    me.sx = (static_cast<float>(width_) - me.sw) / 2.f;
    me.sy = (setup.car ? std::min(contact_row(setup, cam.depth, fb_height()), static_cast<float>(fb_height())) : fb_height()) - me.sh -
            1.f + bounce_;
    me.sy -= std::min(40.f * static_cast<float>(pixel_scale_), (1.f - camera_air_share) * air * scale * (static_cast<float>(fb_height()) / 2.f) / fb_x_unit());
    if (landing_time_ > 0.f) me.sy += 3.f; // squashed by a hard landing
    bool car_visible = setup.car && !attract_;
    if (crash_time_ >= 0.f && setup.car) {
        const CrashPose pose = crash_pose(crash_time_, crash_side_);
        me.angle = pose.angle;
        me.sx += pose.slide;
        me.sy -= pose.lift;
        car_visible = pose.visible;
    }
    if (car_visible) road_sprites_.push_back(me);

    if (use_gles_ && display_ && display_->make_gl_current()) {
        gles_.set_size(width_, fb_height());
        const Daylight scene_light = lit_by(daylight_at(hour_), look.night_glow);
        const Beam* beam_ptr = nullptr;
        Beam beam{};
        if (headlights_ && debug_.headlights) {
            beam.start = setup.distance + 450.f;
            beam.center = static_cast<float>(width_) / 2.f;
            beam.aim = 0.08f * static_cast<float>(shown_steer);
            beam.camera_depth = cam.depth;
            beam.x_scale = fb_x_unit();
            beam.bottom = setup.cockpit ? fb_height() - dashboard_height * pixel_scale_ : fb_height();
            beam_ptr = &beam;
        }
        const Weather* weather_ptr = nullptr;
        if (debug_.weather && !track_.segment_at(world_.get<Transform>(player_).z + cam.player_z()).tunnel)
            weather_ptr = &weather_;
        gles_.render(track_, view, sprites_, road_sprites_, look, scene_light, &background_, hour_, beam_ptr,
                     weather_ptr);
        // Pause / options: full software frame underneath the menus.
        if (paused_ || options_open_ || video_open_ || audio_open_ || debug_open_) {
            background_.render(fb_, look, hour_);
            road_.render(fb_, track_, view, sprites_, road_sprites_);
        } else {
            // Transparent buffer so only HUD / cockpit pixels composite over GLES.
            std::fill(fb_.pixels_mut(), fb_.pixels_mut() + width_ * fb_height(), 0u);
        }
    } else {
        background_.render(fb_, look, hour_);
        road_.render(fb_, track_, view, sprites_, road_sprites_);
    }

    if (scraping_ && vel.speed > 0.f && setup.car) {
        // Sparks flying off the side of the car that scrapes the barrier.
        const float x = static_cast<float>(width_) / 2.f + static_cast<float>(scrape_side_) * me.sw / 2.f;
        for (int i = 0; i < 16; ++i) {
            rng_ = rng_ * 1664525u + 1013904223u;
            const int dx = static_cast<int>((rng_ >> 8) % 11) - 5 + scrape_side_ * 2;
            const int dy = static_cast<int>((rng_ >> 16) % 16);
            const bool bright = (rng_ >> 28) & 1;
            const int px = static_cast<int>(x) + dx, py = fb_height() - 4 * pixel_scale_ - dy + static_cast<int>(bounce_);
            // A short streak trailing away from the barrier, hot end first.
            fb_.put_pixel(px, py, bright ? Color{255, 240, 120} : Color{255, 170, 50});
            fb_.put_pixel(px - scrape_side_, py + 1, Color{255, 130, 30});
            if (i % 2 == 0) fb_.put_pixel(px - 2 * scrape_side_, py + 2, Color{200, 80, 25});
        }
    }
    for (const Particle& p : particles_) {
        // From inside the car only the water of a car wash shows, on the
        // windscreen; the rest is thrown up behind and beside it.
        if (!setup.car && !washing_) break;
        const float fade = p.life / p.max_life;
        if (p.kind == Particle::Kind::Spray) {
            // A drop: one pixel, or a small cluster for the big ones.
            const int x = static_cast<int>(p.x), y = static_cast<int>(p.y);
            fb_.blend_pixel(x, y, p.color, 0.9f * fade);
            if (p.size > 1.4f) {
                fb_.blend_pixel(x + 1, y, p.color, 0.6f * fade);
                fb_.blend_pixel(x, y + 1, p.color, 0.6f * fade);
            }
        } else if (p.kind == Particle::Kind::Dust) {
            const int r = static_cast<int>(p.size);
            for (int dy = -r; dy <= r; ++dy) {
                for (int dx = -r; dx <= r; ++dx) {
                    if (dx * dx + dy * dy > r * r) continue;
                    const int px = static_cast<int>(p.x) + dx, py = static_cast<int>(p.y) + dy;
                    if (bayer4(px, py) < 0.85f * fade) fb_.blend_pixel(px, py, p.color, 0.8f);
                }
            }
        } else {
            fb_.put_pixel(static_cast<int>(p.x), static_cast<int>(p.y), p.color);
            if (fade > 0.5f) fb_.put_pixel(static_cast<int>(p.x) + 1, static_cast<int>(p.y), p.color);
        }
    }
    if (nitro_.burning() && setup.car) {
        for (int side = -1; side <= 1; side += 2) {
            // The pipes at a quarter and three quarters across, low on the
            // body, whatever the car and the view.
            draw_flame(fb_, me.sx + me.sw * (side < 0 ? 0.25f : 0.75f) + static_cast<float>(steer_),
                       me.sy + me.sh * 0.9f,
                       side, nitro_.intensity(), rng_);
        }
    }
    // No rain or snow falls in a tunnel. On the GLES path the particles are
    // drawn into the scene FBO; only fall back to software when that path is
    // not active (pause menus, headless, no GL).
    if (debug_.weather && !track_.segment_at(tr.z + cam.player_z()).tunnel &&
        !(use_gles_ && !paused_ && !options_open_ && !video_open_ && !audio_open_ && !debug_open_))
        weather_.render(fb_);
    if (setup.cockpit) {
        // The dashboard and the wheel, shaking with the car. On a wide
        // screen the dashboard is centred and its outer edges carry on to
        // the sides.
        const Bitmap& dash = sprites_.dashboard(car_model_);
        const int ps = pixel_scale_;
        const int dash_w = dash.w * ps, dash_h = dash.h * ps;
        const int dash_x = (width_ - dash_w) / 2;
        const int dash_y = fb_height() - dashboard_height * ps + static_cast<int>(bounce_);
        for (int y = 0; y < dash_h; ++y) {
            for (int x = 0; x < width_; ++x) {
                const int dx = std::clamp((x - dash_x) / ps, 0, dash.w - 1);
                const int dy = std::clamp(y / ps, 0, dash.h - 1);
                const uint32_t p = dash.px[static_cast<size_t>(dy) * dash.w + dx];
                if (p >> 24) fb_.put_pixel(x, dash_y + y, Color{static_cast<uint8_t>(p >> 16), static_cast<uint8_t>(p >> 8),
                                                                static_cast<uint8_t>(p)});
            }
        }
        const float twitch = spin_time_ > 0.f ? (static_cast<int>(clock_ * 16.f) % 2 ? 0.3f : -0.3f) : 0.f;
        fb_.blit_rotated(sprites_.wheel(driver_), static_cast<float>(dash_x + dashboard_wheel_x * ps),
                         static_cast<float>(fb_height()) + 6.f * static_cast<float>(ps) + bounce_,
                         static_cast<float>(wheel_size * ps), static_cast<float>(wheel_size * ps), wheel_angle_ + twitch);
    }
    if (debug_.mirror) render_mirror();

    // Nightfall: the picture darkened but for its lamps; the headlights
    // light the road ahead again, from the car's front up (above the
    // dashboard in the cockpit).
    const Daylight light = lit_by(daylight_at(hour_), look.night_glow);
    // (The picture before nightfall, for the light the headlights and the
    // street lamps bring back.)
    if (!(use_gles_ && !paused_)) {
    if (headlights_ || !road_.lamps().empty()) day_picture_.assign(fb_.pixels(), fb_.pixels() + width_ * fb_height());
    apply_daylight(fb_, light);
    street_lights(fb_, day_picture_, road_.ground(), light, road_.row_depth(), road_.lamps(), cam.depth, fb_x_unit());
    if (headlights_ && debug_.headlights) {
        Beam beam;
        beam.start = setup.distance + 450.f; // the lamps, at the car's front
        beam.center = static_cast<float>(width_) / 2.f;
        beam.aim = 0.08f * static_cast<float>(shown_steer);
        beam.camera_depth = cam.depth;
        beam.x_scale = fb_x_unit();
        beam.bottom = setup.cockpit ? fb_height() - dashboard_height * pixel_scale_ : fb_height();
        // The car's own body stays dark: its pixels as they were before.
        const int cx0 = std::max(0, static_cast<int>(me.sx)), cx1 = std::min(width_, static_cast<int>(me.sx + me.sw) + 1);
        const int cy0 = std::max(0, static_cast<int>(me.sy)), cy1 = std::min(fb_height(), static_cast<int>(me.sy + me.sh) + 1);
        const bool mask = car_visible && me.angle == 0.f && cx0 < cx1 && cy0 < cy1;
        if (mask) {
            car_night_.clear();
            for (int y = cy0; y < cy1; ++y) {
                car_night_.insert(car_night_.end(), fb_.pixels() + y * width_ + cx0, fb_.pixels() + y * width_ + cx1);
            }
        }
        headlight_beam(fb_, day_picture_, light, road_.row_depth(), beam);
        if (mask) {
            const uint32_t* night = car_night_.data();
            for (int y = cy0; y < cy1; ++y) {
                for (int x = cx0; x < cx1; ++x, ++night) {
                    const int u = static_cast<int>((static_cast<float>(x) + 0.5f - me.sx) / me.sw * static_cast<float>(car.w));
                    const int v = static_cast<int>((static_cast<float>(y) + 0.5f - me.sy) / me.sh * static_cast<float>(car.h));
                    if (car.opaque(u, v)) fb_.pixels_mut()[y * width_ + x] = *night;
                }
            }
        }
    }
    if (flash_time_ > 0.f) {
        // Lightning lights up everything for a moment.
        const float a = std::min(0.75f, flash_time_ * 5.f);
        for (int y = 0; y < fb_height(); ++y) {
            for (int x = 0; x < width_; ++x) fb_.blend_pixel(x, y, Color{0xf0, 0xf4, 0xff}, a);
        }
    }
    } // !use_gles_ night post

    HudState hud;
    hud.speed_fraction = std::abs(vel.speed) / player.max_speed;
    hud.speed_kmh_fraction = std::abs(vel.speed) / base_max_speed_;
    hud.reverse = vel.speed < 0.f;
    hud.signal_left = blink_on_ && (signal_ < 0 || hazards_);
    hud.signal_right = blink_on_ && (signal_ > 0 || hazards_);
    hud.headlights = headlights_;
    if (offer_) {
        hud.offer_title = lot_name(*offer_);
        if (*offer_ == Lot::Dealer || *offer_ == Lot::SportsDealer || *offer_ == Lot::Truckstop ||
            (*offer_ == Lot::Hospital && hospital_ambulance_)) {
            const CarModel& m = car_model(car_model_);
            hud.offer_name = m.name;
            hud.offer_stats = true;
            hud.offer_values[0] = m.top_speed;
            hud.offer_values[1] = m.acceleration;
            hud.offer_values[2] = m.grip;
        } else {
            hud.offer_name = *offer_ == Lot::Motel ? passenger(passenger_).name : driver(driver_).name;
        }
    }
    hud.lap = lap_;
    hud.lap_time = lap_time_;
    {
        const int minutes = static_cast<int>(hour_ * 60.f);
        char clock[8];
        std::snprintf(clock, sizeof clock, "%02d:%02d", minutes / 60 % 24, minutes % 60);
        hud.time_of_day = clock;
    }
    if (car_model_ == taxi_model) {
        hud.taxi = fare_zone_ >= 0 ? "TO " + track_.zones[static_cast<size_t>(fare_zone_)].region
                                   : "FARES " + std::to_string(fares_paid_);
    }
    hud.last_lap = last_lap_;
    hud.best_lap = best_lap_;
    hud.message = paused_ ? std::string() : message_; // the pause menu says what there is to say
    hud.message_visible = std::fmod(clock_, 0.5f) < 0.35f;
    hud.muted = muted_;
    hud.nitro = nitro_.canisters();
    hud.nitro_capacity = nitro_.capacity();
    hud.fuel = fuel_.level();
    hud.map = debug_.map ? &map_ : nullptr;
    // A fork coming up: the routes' names, left and right.
    for (const Branch& br : track_.branches) {
        const int ahead = br.fork - track_.index_at(tr.z + cam.player_z());
        if (ahead > 0 && ahead < 200) {
            hud.fork_left = br.names[0];
            hud.fork_right = br.names[1];
        }
    }
    hud.map_player = track_.index_at(tr.z + cam.player_z());
    hud.map_start = track_.index_at(track_.start_z);
    hud.map_lots = &lots_;
    hud.map_blink = std::fmod(clock_, 0.4f) < 0.2f;
    hud.map_zoom = map_zoomed_ ? 3.f : 1.f;
    hud.fuel_warning = fuel_.level() < Fuel::low && std::fmod(clock_, 0.5f) < 0.3f;
    hud.nitro_burn = nitro_.burn_left();
    if (police_ != INVALID_ENTITY && !chase_.giving_up && chase_.phase != ChasePhase::Leaving) {
        const float gap = signed_gap(world_.get<Transform>(police_).z, tr.z + cam.player_z(), track_.length()) /
                          track_.segment_length;
        hud.chase = true;
        hud.escape = escape_progress(chase_, gap);
        hud.chase_red = static_cast<int>(clock_ / 0.12f) % 2 == 0;
    }
    hud.attract = attract_;
    hud.attract_prompt = std::fmod(clock_, 1.f) < 0.65f;
    if (banner_time_ > 0.f && zone_ >= 0 && !paused_) {
        hud.banner = track_.zones[static_cast<size_t>(zone_)].country;
        hud.banner_sub = track_.zones[static_cast<size_t>(zone_)].region;
    }
    if (debug_.hud) draw_hud(fb_, hud);
}

// The road behind the car, drawn into its own small framebuffer and set into
// the mirror housing at the top of the screen. The road renderer looks back
// from the car with sides kept, which is what a mirror shows; the traffic
// shows its front and billboards their back.
void Game::render_mirror() {
    const auto& tr = world_.get<Transform>(player_);
    const auto& cam = world_.get<Camera>(camera_);
    const float car_z = tr.z + cam.player_z();
    const RoadTheme look = look_at(car_z);

    // Keep the proportions of the main view, which maps a world unit to
    // x_unit pixels across and height/2 pixels up at scale 1.
    const float half_w = static_cast<float>(mir_width()) / 2.f;
    const float y_scale = half_w * (static_cast<float>(fb_height()) / 2.f) / fb_x_unit();
    const float zoom = mirror_depth * half_w / (cam.depth * fb_x_unit());
    background_.render(mirror_fb_, look, BackdropView{mirror_horizon * static_cast<float>(pixel_scale_), zoom, true}, hour_);

    RoadView view;
    view.position = car_z;
    view.player_x = tr.x;
    view.player_y = tr.y + std::max(0.f, vertical_.y - tr.y); // in a jump, from up in the air
    view.camera_height = mirror_camera_height;
    view.camera_depth = mirror_depth;
    view.draw_distance = cam.draw_distance;
    view.fog_density = look.fog_density;
    view.fog_air = atmosphere_air(look);
    {
        const Daylight dl = daylight_at(hour_);
        const float night = (1.f - dl.level) / (1.f - night_level);
        if (night > 0.02f) {
            view.fog_density *= 1.f + 0.65f * night;
            view.fog_air = blend(view.fog_air, Color{0x18, 0x1c, 0x28}, 0.4f * night);
        }
    }
    view.direction = -1;
    view.player_z = 0.f; // the mirror's camera is in the car
    view.horizon = mirror_horizon * static_cast<float>(pixel_scale_);
    view.y_scale = y_scale;
    if (window_wake_time_ > 0.f && daylight_at(hour_).level < 0.5f)
        view.window_wake = 55.f * track_.segment_length;
    mirror_sprites_.clear();
    world_.view<Transform, Traffic>([&](Entity e, Transform& t, Traffic& traffic) {
        RoadSprite s;
        s.z = t.z;
        s.bitmap = traffic.dir > 0 ? &sprites_.vehicle_front(traffic.kind, traffic.style, indicator(e, t, traffic),
                                                             SpriteSheet::tyre_frame(t.z))
                                   : &sprites_.vehicle(traffic.kind, traffic.style, 0, traffic.braking,
                                                       SpriteSheet::tyre_frame(t.z));
        s.offset = t.x;
        s.world_width = vehicle_info(traffic.kind).width;
        mirror_sprites_.push_back(s);
    });
    mirror_road_.render(mirror_fb_, track_, view, sprites_, mirror_sprites_);

    const int mirror_x = (width_ - mir_width()) / 2;
    draw_mirror_frame(fb_, mirror_x, mirror_y * pixel_scale_, mir_width(), mir_height());
    fb_.blit(mirror_fb_, mirror_x, mirror_y * pixel_scale_);
    draw_mirror_sheen(fb_, mirror_x, mirror_y * pixel_scale_, mir_width(), mir_height());
}

} // namespace racer
