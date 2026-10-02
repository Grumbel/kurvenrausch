// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "game.hpp"

#include "drivetrain.hpp"

#include <SDL2/SDL.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iostream>

namespace racer {

namespace {

constexpr int traffic_count = 48;

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
constexpr float follow_range = 6.f;
constexpr float follow_gap = 2.f;
constexpr float follow_closing = 1.5f;
constexpr float follow_width = 0.3f;
constexpr float traffic_brake = 0.6f;
constexpr float traffic_accel = 0.15f;

// Above the top speed (after nitro or a pass boost) the car loses this much
// of the top speed per second until it is back down.
constexpr float overspeed_drag = 0.15f;

// Above this fraction of the top speed the tyres aquaplane in a wet spot and
// keep only this much of their grip.
constexpr float aquaplane_speed = 0.35f;

// How much of the car's height above the road in a jump the camera follows.
constexpr float camera_air_share = 0.7f;
constexpr float aquaplane_grip = 0.35f;

// On an oil slick above this fraction of the top speed the tyres keep only
// this much of their grip.
constexpr float oil_speed = 0.15f;
constexpr float oil_grip = 0.08f;

// Refuelling works on the forecourt below this fraction of the top speed.
constexpr float refuel_speed = 0.08f;
// Stranded with an empty tank this long, the driver pours in a spare can.
constexpr float stranded_seconds = 3.f;
constexpr float spare_can = 0.15f;

// Hitting something solid faster than this fraction of the top speed is a
// crash with the tumbling animation; slower it is a knock that slows the car.
constexpr float crash_speed = 0.4f;
// Where the car's tyres touch the ground on screen, for dust and debris.
constexpr float ground_y = static_cast<float>(Game::height) - 3.f;

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
    : fb_(width, height), track_(build_demo_track()), mirror_fb_(mirror_width, mirror_height),
      weather_(width, height) {
    stations_ = track_.gas_stations();
    map_ = track_map(track_);
    player_ = world_.create();
    world_.add<Transform>(player_);
    world_.add<Velocity>(player_);
    world_.add<Player>(player_, Player::for_segment_length(track_.segment_length));

    camera_ = world_.create();
    world_.add<Camera>(camera_);

    spawn_traffic();
}

void Game::spawn_traffic() {
    std::vector<Entity> old;
    world_.view<Traffic>([&](Entity e, Traffic&) { old.push_back(e); });
    for (Entity e : old) world_.destroy(e);

    const float max_speed = world_.get<Player>(player_).max_speed;
    const float seg_len = track_.segment_length;
    const float n = static_cast<float>(track_.segments.size());
    uint32_t seed = 0x7a3c9e11u;
    auto rnd = [&seed] {
        seed = seed * 1664525u + 1013904223u;
        return static_cast<float>(seed >> 8) / 16777216.f;
    };
    for (int i = 0; i < traffic_count; ++i) {
        const Entity car = world_.create();
        // Keep the start straight clear.
        const float segment = 60.f + rnd() * (n - 80.f);
        const int lanes = track_.look(static_cast<int>(segment)).lanes;
        const float lane = lane_center(lanes, static_cast<int>(rnd() * static_cast<float>(lanes)) % lanes);
        world_.add<Transform>(car, Transform{lane, 0.f, segment * seg_len});
        Traffic traffic;
        traffic.kind = traffic_vehicle(rnd());
        const VehicleInfo& info = vehicle_info(traffic.kind);
        const float speed = max_speed * (info.min_speed + (info.max_speed - info.min_speed) * rnd());
        world_.add<Velocity>(car, Velocity{speed});
        traffic.style = i % info.styles;
        traffic.target_x = lane;
        traffic.cruise = speed;
        world_.add<Traffic>(car, traffic);
    }
}

bool Game::init() {
    display_ = std::make_unique<Display>();
    if (!display_->init("Kurvenrausch", app_id, width, height, window_scale)) return false;
    const Bitmap icon = make_app_icon();
    display_->set_icon(icon.px.data(), icon.w, icon.h);

    input_.init(); // not fatal: the keyboard always works
    audio_.init(synth_); // nor is a missing audio device

    std::cout << "Kurvenrausch: " << track_.segments.size() << " segments, "
              << track_.length() << " units.\n"
              << "Controls: Arrows / WASD or gamepad to drive, R / Start to restart,\n"
              << "          M mute, F11 fullscreen, Esc to quit.\n";
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
    lap_time_ = last_lap_ = best_lap_ = 0.f;
    message_.clear();
    message_time_ = 0.f;
    zone_ = -1;
    banner_time_ = 0.f;
    horn_ = false;
    nitro_.reset();
    nitro_held_ = false;
    wave_time_ = 0.f;
    crash_time_ = -1.f;
    particles_.clear();
    wet_ = aquaplaning_ = oily_ = false;
    spin_time_ = 0.f;
    place_on_road(vertical_, track_.height_at(world_.get<Camera>(camera_).player_z()));
    landing_time_ = 0.f;
    fuel_.reset();
    engine_on_ = true;
    refuelling_ = false;
    stranded_time_ = 0.f;
}

void Game::update_rumble() {
    const auto& vel = world_.get<Velocity>(player_);
    const auto& tr = world_.get<Transform>(player_);
    const float speed_pct = vel.speed / world_.get<Player>(player_).max_speed;
    if (crashed_) {
        input_.rumble(1.f, 0.8f, 250);
    } else if (passed_) {
        input_.rumble(0.2f, 0.6f, 120);
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
    Uint64 prev = SDL_GetPerformanceCounter();
    const double freq = static_cast<double>(SDL_GetPerformanceFrequency());
    float accumulator = 0.f;
    InputState input;

    for (;;) {
        const Uint64 now = SDL_GetPerformanceCounter();
        const float dt = std::min(static_cast<float>((now - prev) / freq), 0.25f);
        prev = now;

        input_.poll(input);
        if (input.quit) break;
        if (input.restart) reset();
        if (input.toggle_fullscreen) display_->toggle_fullscreen();
        if (input.toggle_mute) muted_ = !muted_;

        accumulator += dt;
        crashed_ = false;
        passed_ = false;
        while (accumulator >= fixed_dt_) {
            fixed_update(input, fixed_dt_);
            accumulator -= fixed_dt_;
        }
        update_rumble();

        render();
        display_->present(fb_.pixels());

        if (!display_->vsync()) SDL_Delay(1);
    }
}

float Game::zone_start_position(int index) const {
    const Zone& zone = track_.zones.at(static_cast<size_t>(index));
    // Past the fade into the zone, and the camera sits player_z behind the car.
    const float cam = world_.get<Camera>(camera_).player_z();
    return track_.wrap(static_cast<float>(zone.first_segment + 80) * track_.segment_length - cam);
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
    const float start = opts.zone >= 0 ? zone_start_position(opts.zone) : opts.position;
    world_.get<Transform>(player_).z = track_.wrap(start);
    place_on_road(vertical_, track_.height_at(start + world_.get<Camera>(camera_).player_z()));
    zone_ = -1;
    if (opts.fuel >= 0.f) fuel_.set(opts.fuel);
    std::vector<int16_t> sound;
    constexpr int samples_per_step = Synth::sample_rate / 60; // 735, exactly
    for (int i = 0; i < opts.frames; ++i) {
        InputState in = autopilot();
        if (opts.force_steer && i >= opts.steer_from) in.steer = opts.steer;
        in.horn = opts.horn;
        in.nitro = i == opts.nitro_frame;
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
    render();
    return save_bmp(opts.path, fb_.pixels(), width, height);
}

InputState Game::autopilot() const {
    const auto& tr = world_.get<Transform>(player_);
    const auto& vel = world_.get<Velocity>(player_);
    const auto& player = world_.get<Player>(player_);
    const auto& cam = world_.get<Camera>(camera_);

    const Segment& seg = track_.segment_at(tr.z + cam.player_z());
    const float pct = vel.speed / player.max_speed;
    // Lateral push the curve will apply this tick, relative to one steering tick.
    const float grip = std::max(track_.look_at(tr.z + cam.player_z()).grip, 0.2f);
    const float drift = -pct * seg.curve * player.centrifugal / grip;

    // Low on fuel with a gas station coming up: move over to the right, slow
    // down, pull onto the forecourt and wait there until the tank is full.
    float target_x = 0.f;
    float speed_limit = 1.f;
    if (fuel_.level() < 0.45f || (refuelling_ && !fuel_.full())) {
        const int n = static_cast<int>(track_.segments.size());
        const int here = track_.index_at(tr.z + cam.player_z());
        const bool on = seg.forecourt >= forecourt_width;
        for (int start : stations_) {
            const int ahead = ((start - here) % n + n) % n;
            if (!on && ahead > 120) continue;
            target_x = on || seg.forecourt > 1.3f ? 1.45f : 0.6f;
            speed_limit = on ? refuel_speed * 0.6f : 0.1f + 0.9f * static_cast<float>(ahead) / 120.f;
            break;
        }
    }
    const float wanted = (target_x - tr.x) * 4.f - drift;

    InputState in;
    in.steer = wanted > 0.3f ? 1.f : wanted < -0.3f ? -1.f : 0.f;
    const bool coast = std::abs(drift) > 1.f && std::abs(tr.x) > 0.6f && target_x == 0.f;
    const bool slow = pct > speed_limit;
    in.throttle = coast || slow || pct > speed_limit * 0.9f ? 0.f : 1.f;
    in.brake = coast || slow ? 1.f : 0.f;
    return in;
}

void Game::fixed_update(const InputState& input, float dt) {
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
    const float speed_pct = vel.speed / player.max_speed;
    const float dx = dt * 2.f * speed_pct; // steering is stronger at speed

    // Horn, nitro and the wave after a close pass.
    horn_ = input.horn;
    nitro_.update(dt);
    if (input.nitro && !nitro_held_ && !fuel_.empty()) nitro_.fire();
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

    // Wet or icy roads give the tyres less to bite on: steering has less
    // effect and the car is pushed further out of curves.
    // Ploughing through a wet spot fast, the tyres lose most of their grip
    // too: the car barely steers, slides out of bends, twitches and slows.
    const RoadTheme& look = track_.look_at(tr.z + player_z);
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
    const float grip = std::max(look.grip, 0.2f) * (aquaplaning_ ? aquaplane_grip : 1.f) * (oily_ ? oil_grip : 1.f);
    steer_ = input.steer > 0.3f ? 1 : input.steer < -0.3f ? -1 : 0;
    braking_ = input.brake > 0.1f;
    if (!airborne) {
        tr.x += dx * input.steer * (0.5f + 0.5f * grip);
        tr.x -= dx * speed_pct * seg.curve * player.centrifugal / grip;
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
    if (wet_) spawn_spray(speed_pct);

    weather_.update(look.rain, look.snowfall, -seg.curve * 25.f * speed_pct, speed_pct, dt);

    // Braking overrides the throttle; without either the car coasts down.
    update_fuel(input, dt);
    const float drive = engine_on_ ? input.throttle * (1.f - input.brake) : 0.f;
    float accel = 0.f;
    if (!airborne) {
        accel = player.accel * drive;
        if (input.brake > 0.01f) accel += player.brake * input.brake;
        else accel += player.decel * (1.f - drive);
    }
    accel += player.accel * Nitro::thrust * nitro_.intensity();
    const float speed_before = vel.speed;
    vel.speed += accel * dt;

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
                vel.speed = player.max_speed / 5.f;
                crashed_ = true;
                synth_.trigger_crash(0.55f + 0.45f * speed_pct);
                // Put the car back to the start of the segment it hit.
                const float seg_start = static_cast<float>(track_.index_at(tr.z + player_z)) *
                                        track_.segment_length;
                tr.z = track_.wrap(seg_start - player_z);
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
    if (scraping_) vel.speed -= player.max_speed * 0.5f * dt;

    // Rear-ending traffic: bounce off and drop behind it.
    const int car_segment = track_.index_at(tr.z + player_z);
    const float car_w = player.car_width / track_.half_width_at(tr.z + player_z);
    world_.view<Transform, Velocity, Traffic>([&](Entity, Transform& t, Velocity& v, Traffic& traffic) {
        if (airborne || vel.speed <= v.speed || track_.index_at(t.z) != car_segment) return;
        const float w = vehicle_info(traffic.kind).width / track_.half_width_at(tr.z + player_z);
        if (!overlap(tr.x, car_w, t.x, w * 0.8f)) return;
        vel.speed = v.speed * (v.speed / vel.speed);
        tr.z = track_.wrap(t.z - player_z);
        place_on_road(vertical_, track_.height_at(tr.z + player_z));
        crashed_ = true;
        synth_.trigger_crash(0.4f + 0.4f * speed_pct);
    });

    tr.x = std::clamp(tr.x, -3.f, 3.f);
    const float top = player.max_speed * (nitro_.burning() ? Nitro::top_speed : 1.f);
    vel.speed = std::max(0.f, limit_speed(speed_before, vel.speed, top, overspeed_drag * player.max_speed, dt));
    tr.y = track_.height_at(tr.z + player_z);
    if (landing > 0.f) land(landing);

    // Engine and road shake; rougher off the road. Whole pixels only, the
    // car is pixel art.
    rng_ = rng_ * 1664525u + 1013904223u;
    const float shake = (std::abs(tr.x) > 1.f ? 2.f : 1.f) * vel.speed / player.max_speed;
    bounce_ = !airborne && (rng_ >> 31) && shake > 0.25f ? -std::round(shake) : 0.f;

    update_laps(prev_z + player_z, tr.z + player_z, dt);
    update_traffic(dt);
    check_close_passes();
    update_audio(input, dt);
}

// Burns fuel with the engine's load, refuels on a forecourt, sputters when
// nearly empty and dies when empty; stranded, the driver uses a spare can.
void Game::update_fuel(const InputState& input, float dt) {
    const auto& tr = world_.get<Transform>(player_);
    const auto& vel = world_.get<Velocity>(player_);
    const auto& player = world_.get<Player>(player_);
    const float car_z = tr.z + world_.get<Camera>(camera_).player_z();
    const float speed_pct = vel.speed / player.max_speed;

    const bool at_pump = track_.segment_at(car_z).forecourt >= forecourt_width &&
                         track_.on_forecourt(car_z, tr.x) && speed_pct < refuel_speed;
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

    fuel_.burn(Fuel::load(input.throttle * (1.f - input.brake), drivetrain::rpm(speed_pct)), dt);
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
    const int lanes = track_.look_at(tr.z + world_.get<Camera>(camera_).player_z()).lanes;
    crash_target_x_ = lane_center(lanes, crash_side_ < 0 ? 0 : lanes - 1);
    crashed_ = true;
    nitro_.stop();
    wave_time_ = 0.f;
    synth_.trigger_crash(1.f);

    // Bits of car flying off, and a cloud of dust.
    const float x = static_cast<float>(width) / 2.f;
    const Color chips[] = {{0xd0, 0x18, 0x1c}, {0x88, 0x08, 0x10}, {0x9a, 0x9a, 0xa8}, {0x20, 0x20, 0x24}, {0xa0, 0xc8, 0xe8}};
    for (int i = 0; i < 24; ++i) {
        rng_ = rng_ * 1664525u + 1013904223u;
        const float a = static_cast<float>(rng_ >> 8) / 16777216.f;
        rng_ = rng_ * 1664525u + 1013904223u;
        const float b = static_cast<float>(rng_ >> 8) / 16777216.f;
        particles_.push_back({x + (a - 0.5f) * 60.f, ground_y - 20.f * b,
                              (a - 0.5f) * 320.f + static_cast<float>(crash_side_) * 60.f, -80.f - 220.f * b,
                              0.9f + 0.6f * b, 0.9f + 0.6f * b, 1.f, chips[i % 5], Particle::Kind::Debris});
    }
    spawn_dust(x, ground_y, 10, 1.f + speed_pct);
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
    const RoadTheme& look = track_.look_at(tr.z + player_z);
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
        spawn_dust(static_cast<float>(width) / 2.f + pose.slide, ground_y, 6, 0.5f + strength);
        crashed_ = true;
    }
    scraping_ = false;
    bounce_ = 0.f;
    steer_ = 0;
    braking_ = false;
    wet_ = aquaplaning_ = oily_ = false;
    spin_time_ = 0.f;
    if (crash_time_ >= crash_seconds) {
        crash_time_ = -1.f;
        vel.speed = 0.f;
    }

    update_laps(prev_z + player_z, tr.z + player_z, dt);
    update_traffic(dt);
    check_close_passes();
    update_audio(InputState{}, dt);
}

void Game::spawn_dust(float x, float y, int count, float strength) {
    const RoadTheme& look = track_.look_at(world_.get<Transform>(player_).z + world_.get<Camera>(camera_).player_z());
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
// is then measured from it). The chosen route is announced as the roads
// separate.
void Game::follow_fork(float prev_car_z) {
    auto& tr = world_.get<Transform>(player_);
    const float car_z = tr.z + world_.get<Camera>(camera_).player_z();
    const int seg = track_.index_at(car_z);
    const int index = track_.branch_at(seg);
    if (index < 0) return;
    const Branch& br = track_.branches[static_cast<size_t>(index)];
    if (seg >= br.fork + br.bend) return;
    const float t = std::fmod(car_z, track_.segment_length) / track_.segment_length;
    const float other = track_.branch_offset(seg) + (track_.branch_offset(seg + 1) - track_.branch_offset(seg)) * t;
    if (!std::isnan(other) && nearer_other_road(tr.x, other)) {
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
    spawn_dust(static_cast<float>(width) / 2.f, ground_y, 3 + static_cast<int>(6.f * strength), 0.4f + strength);
    if (strength > 0.4f) {
        crashed_ = true; // a strong rumble
        landing_time_ = 0.2f;
    }
    world_.get<Velocity>(player_).speed *= 1.f - 0.08f * strength;
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
            const float x = static_cast<float>(width) / 2.f + sd * (40.f + 6.f * a);
            particles_.push_back({x, ground_y - 2.f, sd * (40.f + 160.f * a) * speed_pct, -(80.f + 200.f * b) * speed_pct,
                                  0.3f + 0.3f * b, 0.3f + 0.3f * b, 1.f + b, blend(water, Color{0xff, 0xff, 0xff}, b),
                                  Particle::Kind::Spray});
        }
        if (random() < 0.5f) {
            const float a = random();
            particles_.push_back({static_cast<float>(width) / 2.f + sd * (30.f + 14.f * a), ground_y - 4.f,
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
            if (p.y > ground_y + 4.f) p.life = 0.f; // back on the road
        } else if (p.kind == Particle::Kind::Dust) {
            p.vx *= 1.f - 2.f * dt; // dust hangs in the air and spreads
            p.vy *= 1.f - 2.f * dt;
            p.size += 10.f * dt;
        } else {
            p.vy += 600.f * dt; // debris falls
            if (p.y > ground_y) {
                p.y = ground_y;
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
    const RoadTheme& look = track_.look_at(tr.z + player_z);
    const float speed_pct = vel.speed / player.max_speed;

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
    p.splash = wet_ ? std::clamp(speed_pct * 1.3f, 0.f, 1.f) : 0.f;
    if (vertical_.airborne) p.rpm = std::min(1.f, p.rpm + 0.25f * input.throttle); // wheels spinning free
    p.speed = speed_pct;

    // The tyres squeal when the lateral demand (steering plus the push of the
    // bend) gets close to what the road's grip allows, and under hard braking.
    const float grip = std::max(look.grip, 0.2f);
    const float slip = (std::abs(input.steer) * speed_pct +
                        std::abs(seg.curve) * player.centrifugal * speed_pct * speed_pct / grip) /
                       (grip * 1.6f);
    float skid = std::clamp((slip - 0.6f) / 0.4f, 0.f, 1.f);
    skid = std::max(skid, 0.7f * std::clamp((input.brake * speed_pct - 0.6f) / 0.4f, 0.f, 1.f));
    const bool off_road = std::abs(tr.x) > 1.f && !track_.on_forecourt(tr.z + player_z, tr.x);
    p.skid = skid * std::clamp(speed_pct * 5.f, 0.f, 1.f) * (off_road ? 0.3f : 1.f);
    if (oily_) p.skid = 1.f;

    p.gravel = off_road && speed_pct > 0.01f ? std::clamp((std::abs(tr.x) - 1.f) * 8.f, 0.f, 1.f) : 0.f;
    p.scrape = scraping_ && vel.speed > 0.f ? 1.f : 0.f;
    p.rain = look.rain;
    p.horn = horn_ ? 1.f : 0.f;
    p.nitro = nitro_.intensity();
    p.throttle = std::max(p.throttle, p.nitro);
    p.volume = muted_ ? 0.f : 1.f;
    synth_.set_params(p);
}

// Indicator a car shows while changing lanes: -1, +1, or 0 between blinks.
int Game::indicator(Entity e, const Transform& t, const Traffic& traffic) const {
    const float d = traffic.target_x - t.x;
    if (std::abs(d) < 0.02f) return 0;
    const float phase = static_cast<float>(e % 5) * 0.11f; // they don't all blink in step
    return std::fmod(clock_ + phase, 0.7f) < 0.4f ? (d < 0.f ? -1 : 1) : 0;
}

void Game::update_traffic(float dt) {
    struct Mover { Entity e; float x, z, speed; };
    std::vector<Mover> movers;
    const auto& ptr = world_.get<Transform>(player_);
    const float player_world_z = ptr.z + world_.get<Camera>(camera_).player_z();
    movers.push_back({player_, ptr.x, player_world_z, world_.get<Velocity>(player_).speed});
    world_.view<Transform, Velocity, Traffic>([&](Entity e, Transform& t, Velocity& v, Traffic&) {
        movers.push_back({e, t.x, t.z, v.speed});
    });

    const float look_ahead = 6.f * track_.segment_length;
    auto distance_ahead = [&](float from, float to) { return track_.wrap(to - from); };
    // Is anyone within `range` ahead or behind near lateral position x?
    auto lane_busy = [&](Entity self, float z, float x, float range) {
        for (const Mover& m : movers) {
            if (m.e == self || std::abs(m.x - x) > 0.4f) continue;
            const float d = distance_ahead(z, m.z);
            if (d < range || track_.length() - d < range * 0.5f) return true;
        }
        return false;
    };

    const float max_speed = world_.get<Player>(player_).max_speed;
    world_.view<Transform, Velocity, Traffic>([&](Entity e, Transform& t, Velocity& v, Traffic& traffic) {
        // The road may have a different number of lanes here than where the
        // car was heading: aim for the nearest lane that exists.
        const int lanes = track_.look_at(t.z).lanes;
        const float spacing = 2.f / static_cast<float>(lanes);
        {
            float best = lane_center(lanes, 0);
            for (int i = 1; i < lanes; ++i) {
                const float lane = lane_center(lanes, i);
                if (std::abs(lane - traffic.target_x) < std::abs(best - traffic.target_x)) best = lane;
            }
            traffic.target_x = best;
        }

        // Honked at from behind while in the player's way: pull over to the
        // nearest free lane out of the player's line, in a hurry.
        if (horn_ && distance_ahead(player_world_z, t.z) < honk_range * track_.segment_length &&
            std::abs(t.x - ptr.x) < honk_clearance && std::abs(traffic.target_x - ptr.x) < honk_clearance) {
            std::vector<bool> busy(static_cast<size_t>(lanes));
            for (int i = 0; i < lanes; ++i) busy[static_cast<size_t>(i)] = lane_busy(e, t.z, lane_center(lanes, i), look_ahead);
            const int lane = yield_lane(lanes, t.x, ptr.x, honk_clearance, busy);
            if (lane >= 0) {
                traffic.target_x = lane_center(lanes, lane);
                traffic.startled = startled_seconds;
            }
        }
        if (traffic.startled > 0.f) traffic.startled -= dt;

        // Blocked by something slower ahead in this lane? Pull out if the
        // neighbouring lane is clear.
        bool blocked = false;
        for (const Mover& m : movers) {
            if (m.e == e || std::abs(m.x - t.x) > 0.5f || m.speed >= v.speed) continue;
            if (distance_ahead(t.z, m.z) < look_ahead) { blocked = true; break; }
        }
        if (blocked && std::abs(t.x - traffic.target_x) < 0.05f) {
            for (int i = 0; i < lanes; ++i) {
                const float lane = lane_center(lanes, i);
                if (std::abs(lane - t.x) < 0.15f * spacing || std::abs(lane - t.x) > 1.1f * spacing) continue;
                if (!lane_busy(e, t.z, lane, look_ahead)) { traffic.target_x = lane; break; }
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
                           : traffic.cruise;
        for (const Mover& m : movers) {
            if (m.e == e || std::abs(m.x - t.x) > follow_width) continue;
            const float d = distance_ahead(t.z, m.z);
            if (d < follow_range * track_.segment_length) {
                wanted = std::min(wanted, follow_speed(traffic.cruise, m.speed, d,
                                                       follow_gap * track_.segment_length, follow_closing));
            }
        }
        // Small trims while following steadily don't light the brake lights.
        traffic.braking = v.speed - wanted > 0.02f * max_speed;
        if (wanted < v.speed) v.speed = std::max(wanted, v.speed - traffic_brake * max_speed * dt);
        else v.speed = std::min(wanted, v.speed + traffic_accel * max_speed * dt);
        t.z = track_.wrap(t.z + v.speed * dt);
    });
}

void Game::update_laps(float prev_z, float z, float dt) {
    clock_ += dt;

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
        const bool record = best_lap_ == 0.f || lap_time_ < best_lap_;
        if (record) best_lap_ = lap_time_;
        show_message(record && lap_ > 1 ? "NEW RECORD" : "LAP " + std::to_string(lap_ + 1), 2.5f);
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

    const RoadTheme& look = track_.look_at(tr.z + cam.player_z());
    background_.render(fb_, look);

    RoadView view;
    // In the air the camera rises with most of the car's height, and the car
    // lifts on screen by the rest, so the road visibly falls away beneath it.
    const float air = vertical_.y - tr.y;
    view.position = tr.z;
    view.player_x = tr.x;
    view.player_y = tr.y + camera_air_share * air;
    view.camera_height = cam.height;
    view.camera_depth = cam.depth;
    view.player_z = cam.player_z();
    view.draw_distance = cam.draw_distance;
    view.fog_density = look.fog_density;
    road_sprites_.clear();
    world_.view<Transform, Traffic>([&](Entity e, Transform& t, Traffic& traffic) {
        RoadSprite s;
        s.z = t.z;
        s.bitmap = &sprites_.vehicle(traffic.kind, traffic.style, indicator(e, t, traffic), traffic.braking,
                                     SpriteSheet::tyre_frame(t.z));
        s.offset = t.x;
        s.world_width = vehicle_info(traffic.kind).width;
        road_sprites_.push_back(s);
    });

    // The player's car sits centred, its tyres on the bottom screen row. It
    // is drawn at the projection scale of player_z, which maps car_width to
    // the sprite's native size, so the pixel art is shown 1:1.
    // Twitching after an oil slick, the car flicks from one side to the other.
    const int shown_steer = spin_time_ > 0.f ? (static_cast<int>(clock_ * 16.f) % 2 ? 1 : -1) : steer_;
    const Bitmap& car = sprites_.player(shown_steer, wave_time_ > 0.f ? wave_side_ : 0,
                                        static_cast<int>(clock_ / 0.15f) & 1, braking_,
                                        SpriteSheet::tyre_frame(wheel_distance_));
    const float scale = cam.depth / cam.player_z() * (width / 2.f);
    RoadSprite me;
    me.z = tr.z + cam.player_z();
    me.bitmap = &car;
    me.fixed = true;
    me.sw = player.car_width * scale;
    me.sh = me.sw * static_cast<float>(car.h) / static_cast<float>(car.w);
    me.sx = (width - me.sw) / 2.f;
    me.sy = height - me.sh - 1.f + bounce_;
    me.sy -= std::min(40.f, (1.f - camera_air_share) * air * cam.depth / cam.player_z() * (height / 2.f));
    if (landing_time_ > 0.f) me.sy += 3.f; // squashed by a hard landing
    bool car_visible = true;
    if (crash_time_ >= 0.f) {
        const CrashPose pose = crash_pose(crash_time_, crash_side_);
        me.angle = pose.angle;
        me.sx += pose.slide;
        me.sy -= pose.lift;
        car_visible = pose.visible;
    }
    if (car_visible) road_sprites_.push_back(me);

    road_.render(fb_, track_, view, sprites_, road_sprites_);

    if (scraping_ && vel.speed > 0.f) {
        // Sparks flying off the side of the car that scrapes the barrier.
        const float x = static_cast<float>(width) / 2.f + static_cast<float>(scrape_side_) * me.sw / 2.f;
        for (int i = 0; i < 16; ++i) {
            rng_ = rng_ * 1664525u + 1013904223u;
            const int dx = static_cast<int>((rng_ >> 8) % 11) - 5 + scrape_side_ * 2;
            const int dy = static_cast<int>((rng_ >> 16) % 16);
            const bool bright = (rng_ >> 28) & 1;
            const int px = static_cast<int>(x) + dx, py = height - 4 - dy + static_cast<int>(bounce_);
            // A short streak trailing away from the barrier, hot end first.
            fb_.put_pixel(px, py, bright ? Color{255, 240, 120} : Color{255, 170, 50});
            fb_.put_pixel(px - scrape_side_, py + 1, Color{255, 130, 30});
            if (i % 2 == 0) fb_.put_pixel(px - 2 * scrape_side_, py + 2, Color{200, 80, 25});
        }
    }
    for (const Particle& p : particles_) {
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
    if (nitro_.burning()) {
        for (int side = -1; side <= 1; side += 2) {
            draw_flame(fb_, me.sx + SpriteSheet::exhaust_x(steer_, side), me.sy + SpriteSheet::exhaust_y,
                       side, nitro_.intensity(), rng_);
        }
    }
    weather_.render(fb_);
    render_mirror();

    HudState hud;
    hud.speed_fraction = vel.speed / player.max_speed;
    hud.lap = lap_;
    hud.lap_time = lap_time_;
    hud.last_lap = last_lap_;
    hud.best_lap = best_lap_;
    hud.message = message_;
    hud.message_visible = std::fmod(clock_, 0.5f) < 0.35f;
    hud.muted = muted_;
    hud.nitro = nitro_.canisters();
    hud.fuel = fuel_.level();
    hud.map = &map_;
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
    hud.map_stations = &stations_;
    hud.map_blink = std::fmod(clock_, 0.4f) < 0.2f;
    hud.fuel_warning = fuel_.level() < Fuel::low && std::fmod(clock_, 0.5f) < 0.3f;
    hud.nitro_burn = nitro_.burn_left();
    if (banner_time_ > 0.f && zone_ >= 0) {
        hud.banner = track_.zones[static_cast<size_t>(zone_)].country;
        hud.banner_sub = track_.zones[static_cast<size_t>(zone_)].region;
    }
    draw_hud(fb_, hud);
}

// The road behind the car, drawn into its own small framebuffer and set into
// the mirror housing at the top of the screen. The road renderer looks back
// from the car with sides kept, which is what a mirror shows; the traffic
// shows its front and billboards their back.
void Game::render_mirror() {
    const auto& tr = world_.get<Transform>(player_);
    const auto& cam = world_.get<Camera>(camera_);
    const float car_z = tr.z + cam.player_z();
    const RoadTheme& look = track_.look_at(car_z);

    // Keep the proportions of the main view, which maps a world unit to
    // width/2 pixels across and height/2 pixels up at scale 1.
    const float half_w = static_cast<float>(mirror_width) / 2.f;
    const float y_scale = half_w * static_cast<float>(height) / static_cast<float>(width);
    const float zoom = mirror_depth * half_w / (cam.depth * static_cast<float>(width) / 2.f);
    background_.render(mirror_fb_, look, BackdropView{mirror_horizon, zoom, true});

    RoadView view;
    view.position = car_z;
    view.player_x = tr.x;
    view.player_y = tr.y + std::max(0.f, vertical_.y - tr.y); // in a jump, from up in the air
    view.camera_height = mirror_camera_height;
    view.camera_depth = mirror_depth;
    view.draw_distance = cam.draw_distance;
    view.fog_density = look.fog_density;
    view.direction = -1;
    view.player_z = 0.f; // the mirror's camera is in the car
    view.horizon = mirror_horizon;
    view.y_scale = y_scale;
    mirror_sprites_.clear();
    world_.view<Transform, Traffic>([&](Entity e, Transform& t, Traffic& traffic) {
        RoadSprite s;
        s.z = t.z;
        s.bitmap = &sprites_.vehicle_front(traffic.kind, traffic.style, indicator(e, t, traffic),
                                           SpriteSheet::tyre_frame(t.z));
        s.offset = t.x;
        s.world_width = vehicle_info(traffic.kind).width;
        mirror_sprites_.push_back(s);
    });
    mirror_road_.render(mirror_fb_, track_, view, sprites_, mirror_sprites_);

    draw_mirror_frame(fb_, mirror_x, mirror_y, mirror_width, mirror_height);
    fb_.blit(mirror_fb_, mirror_x, mirror_y);
    draw_mirror_sheen(fb_, mirror_x, mirror_y, mirror_width, mirror_height);
}

} // namespace racer
