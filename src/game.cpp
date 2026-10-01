// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "game.hpp"

#include <SDL2/SDL.h>
#include <algorithm>
#include <cmath>
#include <iostream>

namespace racer {

namespace {

constexpr int traffic_count = 48;
constexpr float lane_offsets[] = {-2.f / 3.f, 0.f, 2.f / 3.f};

// Do the intervals [c1 - w1/2, c1 + w1/2] and [c2 - w2/2, c2 + w2/2] overlap?
bool overlap(float c1, float w1, float c2, float w2) {
    return std::abs(c1 - c2) * 2.f < w1 + w2;
}

} // namespace

Game::Game() : fb_(width, height), track_(build_demo_track()), weather_(width, height) {
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
        const float lane = lane_offsets[static_cast<int>(rnd() * 3.f) % 3];
        world_.add<Transform>(car, Transform{lane, 0.f, segment * seg_len});
        world_.add<Velocity>(car, Velocity{max_speed * (0.25f + 0.35f * rnd())});
        world_.add<Traffic>(car, Traffic{i % SpriteSheet::traffic_styles, lane});
    }
}

bool Game::init() {
    display_ = std::make_unique<Display>();
    if (!display_->init("Kurvenrausch", width, height, window_scale)) return false;

    input_.init(); // not fatal: the keyboard always works

    std::cout << "Kurvenrausch: " << track_.segments.size() << " segments, "
              << track_.length() << " units.\n"
              << "Controls: Arrows / WASD or gamepad to drive, R / Start to restart,\n"
              << "          F11 fullscreen, Esc to quit.\n";
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
}

void Game::update_rumble() {
    const auto& vel = world_.get<Velocity>(player_);
    const auto& tr = world_.get<Transform>(player_);
    const float speed_pct = vel.speed / world_.get<Player>(player_).max_speed;
    if (crashed_) {
        input_.rumble(1.f, 0.8f, 250);
    } else if (scraping_ && speed_pct > 0.05f) {
        input_.rumble(0.6f, 0.6f, 60);
    } else if (std::abs(tr.x) > 1.f && speed_pct > 0.05f) {
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

        accumulator += dt;
        crashed_ = false;
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

bool Game::screenshot(const ScreenshotOptions& opts) {
    world_.get<Transform>(player_).z = track_.wrap(opts.position);
    for (int i = 0; i < opts.frames; ++i) {
        InputState in = autopilot();
        if (opts.force_steer) in.steer = opts.steer;
        fixed_update(in, fixed_dt_);
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
    const float wanted = -tr.x * 4.f - drift;

    InputState in;
    in.steer = wanted > 0.3f ? 1.f : wanted < -0.3f ? -1.f : 0.f;
    const bool coast = std::abs(drift) > 1.f && std::abs(tr.x) > 0.6f;
    in.throttle = coast ? 0.f : 1.f;
    in.brake = coast ? 1.f : 0.f;
    return in;
}

void Game::fixed_update(const InputState& input, float dt) {
    auto& tr = world_.get<Transform>(player_);
    auto& vel = world_.get<Velocity>(player_);
    const auto& player = world_.get<Player>(player_);
    const auto& cam = world_.get<Camera>(camera_);
    const float player_z = cam.player_z();

    const Segment& seg = track_.segment_at(tr.z + player_z);
    const float speed_pct = vel.speed / player.max_speed;
    const float dx = dt * 2.f * speed_pct; // steering is stronger at speed

    const float prev_z = tr.z;
    tr.z = track_.wrap(tr.z + dt * vel.speed);
    background_.update(seg.curve, dt * vel.speed / track_.segment_length, dt);

    // Wet or icy roads give the tyres less to bite on: steering has less
    // effect and the car is pushed further out of curves.
    const RoadTheme& look = track_.look_at(tr.z + player_z);
    const float grip = std::max(look.grip, 0.2f);
    steer_ = input.steer > 0.3f ? 1 : input.steer < -0.3f ? -1 : 0;
    tr.x += dx * input.steer * (0.5f + 0.5f * grip);
    tr.x -= dx * speed_pct * seg.curve * player.centrifugal / grip;

    weather_.update(look.rain, look.snowfall, -seg.curve * 25.f * speed_pct, dt);

    // Braking overrides the throttle; without either the car coasts down.
    const float drive = input.throttle * (1.f - input.brake);
    float accel = player.accel * drive;
    if (input.brake > 0.01f) accel += player.brake * input.brake;
    else accel += player.decel * (1.f - drive);
    vel.speed += accel * dt;

    if (std::abs(tr.x) > 1.f) {
        if (vel.speed > player.offroad_limit) vel.speed += player.offroad_decel * dt;

        // Crash into solid roadside objects on the car's segment.
        const float car_w = player.car_width / track_.road_width;
        for (const RoadsideObject& obj : seg.scenery) {
            const SceneryInfo& info = scenery_info(obj.kind);
            if (!info.solid) continue;
            const float w = info.width / track_.road_width;
            const float center = info.centered ? obj.offset
                                               : obj.offset + (obj.offset < 0.f ? -w : w) / 2.f;
            if (overlap(tr.x, car_w, center, w)) {
                vel.speed = player.max_speed / 5.f;
                crashed_ = true;
                // Put the car back to the start of the segment it hit.
                const float seg_start = static_cast<float>(track_.index_at(tr.z + player_z)) *
                                        track_.segment_length;
                tr.z = track_.wrap(seg_start - player_z);
                break;
            }
        }
    }

    // Rails and cliffs stop the car; leaning on them scrapes the speed away.
    scraping_ = false;
    const float car_half = player.car_width / track_.road_width / 2.f;
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
    const float car_w = player.car_width / track_.road_width;
    world_.view<Transform, Velocity, Traffic>([&](Entity, Transform& t, Velocity& v, Traffic&) {
        if (vel.speed <= v.speed || track_.index_at(t.z) != car_segment) return;
        if (!overlap(tr.x, car_w, t.x, car_w * 0.8f)) return;
        vel.speed = v.speed * (v.speed / vel.speed);
        tr.z = track_.wrap(t.z - player_z);
        crashed_ = true;
    });

    tr.x = std::clamp(tr.x, -3.f, 3.f);
    vel.speed = std::clamp(vel.speed, 0.f, player.max_speed);
    tr.y = track_.height_at(tr.z + player_z);

    // Engine and road shake; rougher off the road. Whole pixels only, the
    // car is pixel art.
    rng_ = rng_ * 1664525u + 1013904223u;
    const float shake = (std::abs(tr.x) > 1.f ? 2.f : 1.f) * vel.speed / player.max_speed;
    bounce_ = (rng_ >> 31) && shake > 0.25f ? -std::round(shake) : 0.f;

    update_laps(prev_z + player_z, tr.z + player_z, dt);
    update_traffic(dt);
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

    world_.view<Transform, Velocity, Traffic>([&](Entity e, Transform& t, Velocity& v, Traffic& traffic) {
        // Blocked by something slower ahead in this lane? Pull out if the
        // neighbouring lane is clear.
        bool blocked = false;
        for (const Mover& m : movers) {
            if (m.e == e || std::abs(m.x - t.x) > 0.5f || m.speed >= v.speed) continue;
            if (distance_ahead(t.z, m.z) < look_ahead) { blocked = true; break; }
        }
        if (blocked && std::abs(t.x - traffic.target_x) < 0.05f) {
            for (float lane : lane_offsets) {
                if (std::abs(lane - t.x) < 0.1f || std::abs(lane - t.x) > 0.7f) continue;
                if (!lane_busy(e, t.z, lane, look_ahead)) { traffic.target_x = lane; break; }
            }
        }

        const float step = 0.8f * dt;
        t.x += std::clamp(traffic.target_x - t.x, -step, step);
        t.z = track_.wrap(t.z + v.speed * dt);
    });
}

void Game::update_laps(float prev_z, float z, float dt) {
    clock_ += dt;
    if (race_started_) lap_time_ += dt;
    if (message_time_ > 0.f) {
        message_time_ -= dt;
        if (message_time_ <= 0.f) message_.clear();
    }

    // Distance travelled past the start line; it drops when the line is crossed.
    const float before = track_.wrap(prev_z - track_.start_z);
    const float after = track_.wrap(z - track_.start_z);
    if (after >= before) return;

    if (race_started_) {
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
    view.position = tr.z;
    view.player_x = tr.x;
    view.player_y = tr.y;
    view.camera_height = cam.height;
    view.camera_depth = cam.depth;
    view.player_z = cam.player_z();
    view.draw_distance = cam.draw_distance;
    view.fog_density = look.fog_density;
    road_sprites_.clear();
    world_.view<Transform, Traffic>([&](Entity, Transform& t, Traffic& traffic) {
        RoadSprite s;
        s.z = t.z;
        s.bitmap = &sprites_.traffic(traffic.style);
        s.offset = t.x;
        s.world_width = player.car_width;
        road_sprites_.push_back(s);
    });

    // The player's car sits centred, its tyres on the bottom screen row. It
    // is drawn at the projection scale of player_z, which maps car_width to
    // the sprite's native size, so the pixel art is shown 1:1.
    const Bitmap& car = sprites_.player(steer_);
    const float scale = cam.depth / cam.player_z() * (width / 2.f);
    RoadSprite me;
    me.z = tr.z + cam.player_z();
    me.bitmap = &car;
    me.fixed = true;
    me.sw = player.car_width * scale;
    me.sh = me.sw * static_cast<float>(car.h) / static_cast<float>(car.w);
    me.sx = (width - me.sw) / 2.f;
    me.sy = height - me.sh - 1.f + bounce_;
    road_sprites_.push_back(me);

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
    weather_.render(fb_);

    HudState hud;
    hud.speed_fraction = vel.speed / player.max_speed;
    hud.lap = lap_;
    hud.lap_time = lap_time_;
    hud.last_lap = last_lap_;
    hud.best_lap = best_lap_;
    hud.message = message_;
    hud.message_visible = std::fmod(clock_, 0.5f) < 0.35f;
    draw_hud(fb_, hud);
}

} // namespace racer
