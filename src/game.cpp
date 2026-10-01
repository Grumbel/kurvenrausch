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

Game::Game() : fb_(width, height), track_(build_demo_track()) {
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

    std::cout << "Kurvenrausch: " << track_.segments.size() << " segments, "
              << track_.length() << " units.\n"
              << "Controls: Arrows / WASD to drive, R to restart, F11 fullscreen, Esc to quit.\n";
    return true;
}

void Game::reset() {
    world_.get<Transform>(player_) = Transform{};
    world_.get<Velocity>(player_) = Velocity{};
    background_.reset();
    spawn_traffic();
    race_started_ = false;
    lap_ = 0;
    lap_time_ = last_lap_ = best_lap_ = 0.f;
    message_.clear();
    message_time_ = 0.f;
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
        while (accumulator >= fixed_dt_) {
            fixed_update(input, fixed_dt_);
            accumulator -= fixed_dt_;
        }

        render();
        display_->present(fb_.pixels());

        if (!display_->vsync()) SDL_Delay(1);
    }
}

bool Game::screenshot(const ScreenshotOptions& opts) {
    world_.get<Transform>(player_).z = track_.wrap(opts.position);
    for (int i = 0; i < opts.frames; ++i) fixed_update(autopilot(), fixed_dt_);
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
    const float drift = -pct * seg.curve * player.centrifugal;
    const float wanted = -tr.x * 4.f - drift;

    InputState in;
    in.right = wanted > 0.3f;
    in.left = wanted < -0.3f;
    in.up = !(std::abs(drift) > 1.f && std::abs(tr.x) > 0.6f);
    in.down = !in.up;
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

    steer_ = (input.right ? 1 : 0) - (input.left ? 1 : 0);
    tr.x += dx * static_cast<float>(steer_);
    tr.x -= dx * speed_pct * seg.curve * player.centrifugal;

    if (input.up) vel.speed += player.accel * dt;
    else if (input.down) vel.speed += player.brake * dt;
    else vel.speed += player.decel * dt;

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
                // Put the car back to the start of the segment it hit.
                const float seg_start = static_cast<float>(track_.index_at(tr.z + player_z)) *
                                        track_.segment_length;
                tr.z = track_.wrap(seg_start - player_z);
                break;
            }
        }
    }

    // Rear-ending traffic: bounce off and drop behind it.
    const int car_segment = track_.index_at(tr.z + player_z);
    const float car_w = player.car_width / track_.road_width;
    world_.view<Transform, Velocity, Traffic>([&](Entity, Transform& t, Velocity& v, Traffic&) {
        if (vel.speed <= v.speed || track_.index_at(t.z) != car_segment) return;
        if (!overlap(tr.x, car_w, t.x, car_w * 0.8f)) return;
        vel.speed = v.speed * (v.speed / vel.speed);
        tr.z = track_.wrap(t.z - player_z);
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

    background_.render(fb_, track_.theme);

    RoadView view;
    view.position = tr.z;
    view.player_x = tr.x;
    view.player_y = tr.y;
    view.camera_height = cam.height;
    view.camera_depth = cam.depth;
    view.player_z = cam.player_z();
    view.draw_distance = cam.draw_distance;
    view.fog_density = cam.fog_density;
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
