#include "game.hpp"

#include <SDL2/SDL.h>
#include <algorithm>
#include <cmath>
#include <iostream>

namespace racer {

Game::Game(int width, int height)
    : width_(width), height_(height), renderer_(width, height) {}

void Game::setup_world() {
    road_.build_demo_track(track_);

    player_ = world_.create();
    world_.add<Transform>(player_);
    world_.add<Velocity>(player_);
    world_.add<Player>(player_);

    camera_ = world_.create();
    world_.add<Camera>(camera_);

    projected_.resize(track_.segments.size());
}

bool Game::init() {
    display_ = std::make_unique<Display>();
    if (!display_->init("Kurvenrausch", width_, height_, 1)) return false;

    setup_world();

    std::cout << "Kurvenrausch ready.\n"
              << "Controls: Arrows / WASD to drive, R to restart, F11 fullscreen, Esc to quit.\n";
    return true;
}

void Game::reset() {
    world_.get<Transform>(player_) = Transform{};
    world_.get<Velocity>(player_) = Velocity{};
    lap_ = 1;
}

void Game::run() {
    Uint64 prev = SDL_GetPerformanceCounter();
    const double freq = static_cast<double>(SDL_GetPerformanceFrequency());
    float accumulator = 0.f;
    InputState input;

    for (;;) {
        const Uint64 now = SDL_GetPerformanceCounter();
        float dt = static_cast<float>((now - prev) / freq);
        prev = now;
        dt = std::min(dt, 0.25f); // don't try to catch up after a stall

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
        display_->present(renderer_.pixels());

        if (!display_->vsync()) SDL_Delay(1);
    }
}

bool Game::screenshot(const std::string& path, int frames) {
    setup_world();
    InputState input;
    input.up = true;
    for (int i = 0; i < frames; ++i) fixed_update(input, fixed_dt_);
    render();
    return save_bmp(path, renderer_.pixels(), width_, height_);
}

void Game::fixed_update(const InputState& input, float dt) {
    auto& player = world_.get<Player>(player_);
    auto& vel = world_.get<Velocity>(player_);
    auto& tr = world_.get<Transform>(player_);

    if (input.up) {
        vel.speed += player.accel * dt;
    } else if (input.down) {
        vel.speed -= player.brake * dt;
    } else {
        vel.speed -= 30.f * dt; // drag
    }
    vel.speed = std::clamp(vel.speed, 0.f, player.max_speed);

    float steer = 0.f;
    if (input.left)  steer -= 1.f;
    if (input.right) steer += 1.f;

    const int seg_idx = track_.index_from_z(tr.z);
    const float curve = track_.get(seg_idx).curve;
    const float centrifugal = curve * vel.speed * player.centrifugal * 0.001f;
    tr.x += (steer * player.steer_speed - centrifugal) * (vel.speed / player.max_speed) * dt * 2.f;
    tr.x = std::clamp(tr.x, -1.5f, 1.5f);

    tr.z += vel.speed * dt;
    if (tr.z >= track_.total_length) {
        tr.z -= track_.total_length;
        ++lap_;
    }
    tr.y = track_.get(seg_idx).y;
}

void Game::render() {
    renderer_.begin_frame();

    const auto& cam = world_.get<Camera>(camera_);
    const auto& tr = world_.get<Transform>(player_);
    const auto& vel = world_.get<Velocity>(player_);

    renderer_.draw_background(tr.x * 50.f + tr.z * 0.01f, 0.f);

    float max_y = 0.f;
    road_.project_segments(track_, tr.z, tr.x, cam, width_, height_, projected_, max_y);
    road_.render(renderer_, track_, projected_, tr.z, tr.x, cam);

    const float car_x = width_ / 2.f + tr.x * (width_ * 0.3f);
    const float car_y = height_ - 60.f;
    renderer_.draw_player_car(car_x, car_y, tr.x * 0.5f, vel.speed);
    renderer_.draw_hud(vel.speed, tr.z, lap_);
}

} // namespace racer
