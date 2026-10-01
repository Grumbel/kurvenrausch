#include "game.hpp"
#include <SDL2/SDL.h>
#include <cmath>
#include <algorithm>
#include <iostream>

namespace racer {

Game::Game(int width, int height) : width_(width), height_(height) {}

Game::~Game() { shutdown(); }

bool Game::init() {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) < 0) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << "\n";
        return false;
    }

    window_ = SDL_CreateWindow(
        "Bitmap Racer - Classic Pseudo-3D",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        width_, height_,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );
    if (!window_) {
        std::cerr << "Window creation failed: " << SDL_GetError() << "\n";
        return false;
    }

    renderer_ = std::make_unique<Renderer>(width_, height_);
    if (!renderer_->init(window_)) {
        std::cerr << "Renderer init failed\n";
        return false;
    }

    input_ = std::make_unique<Input>();

    // Data-driven track
    road_.build_demo_track(track_);

    // ECS: player entity
    player_ = world_.create();
    world_.add<Transform>(player_, Transform{0.f, 0.f, 0.f});
    world_.add<Velocity>(player_, Velocity{0.f, 0.f});
    world_.add<Player>(player_);

    // Camera entity (follows player)
    camera_ = world_.create();
    world_.add<Camera>(camera_);
    world_.add<Transform>(camera_, Transform{0.f, 0.f, 0.f});

    projected_.resize(track_.segments.size());

    std::cout << "Bitmap Racer ready.\n"
              << "Controls: Arrows / WASD to drive, Esc to quit, R to restart.\n"
              << "Track length: " << track_.total_length << " units, "
              << track_.segments.size() << " segments.\n";
    return true;
}

void Game::shutdown() {
    renderer_.reset();
    if (window_) {
        SDL_DestroyWindow(window_);
        window_ = nullptr;
    }
    SDL_Quit();
}

void Game::run() {
    Uint64 prev = SDL_GetPerformanceCounter();
    const double freq = static_cast<double>(SDL_GetPerformanceFrequency());

    while (running_) {
        Uint64 now = SDL_GetPerformanceCounter();
        float dt = static_cast<float>((now - prev) / freq);
        prev = now;
        if (dt > 0.05f) dt = 0.05f; // spiral of death guard

        InputState state;
        input_->poll(state);
        if (state.quit) running_ = false;
        if (state.restart) {
            auto& t = world_.get<Transform>(player_);
            auto& v = world_.get<Velocity>(player_);
            t.z = 0.f; t.x = 0.f;
            v.speed = 0.f;
            lap_ = 1;
        }

        // Input -> player
        auto& player_comp = world_.get<Player>(player_);
        auto& vel = world_.get<Velocity>(player_);
        auto& tr = world_.get<Transform>(player_);

        if (state.up) {
            vel.speed += player_comp.accel * dt;
        } else if (state.down) {
            vel.speed -= player_comp.brake * dt;
        } else {
            vel.speed -= 30.f * dt; // drag
        }
        vel.speed = std::clamp(vel.speed, 0.f, player_comp.max_speed);

        float steer = 0.f;
        if (state.left)  steer -= 1.f;
        if (state.right) steer += 1.f;

        // Centrifugal force from current curve
        int seg_idx = track_.find_segment_index(tr.z);
        float curve = track_.get(seg_idx).curve;
        float centrifugal = curve * vel.speed * player_comp.centrifugal * 0.001f;
        tr.x += (steer * player_comp.steer - centrifugal) * (vel.speed / player_comp.max_speed) * dt * 2.f;
        tr.x = std::clamp(tr.x, -1.5f, 1.5f);

        // Advance along track
        tr.z += vel.speed * dt;
        if (tr.z >= track_.total_length) {
            tr.z -= track_.total_length;
            ++lap_;
        }

        // Camera follows
        auto& cam_tr = world_.get<Transform>(camera_);
        cam_tr.z = tr.z;
        cam_tr.x = tr.x;
        // Smooth camera height a bit over hills
        float seg_y = track_.get(seg_idx).y;
        cam_tr.y = seg_y;

        accumulator_ += dt;
        while (accumulator_ >= fixed_dt_) {
            fixed_update(fixed_dt_);
            accumulator_ -= fixed_dt_;
        }

        render();
        time_ += dt;
    }
}

void Game::fixed_update(float /*dt*/) {
    // Future: physics, AI traffic, collisions
}

void Game::render() {
    renderer_->begin_frame();

    auto& cam = world_.get<Camera>(camera_);
    auto& tr = world_.get<Transform>(player_);
    auto& vel = world_.get<Velocity>(player_);

    // Background with mild parallax
    float sky_off = tr.x * 50.f + tr.z * 0.01f;
    renderer_->draw_background(sky_off, 0.f);

    // Project + draw road
    float max_y = 0.f;
    road_.project_segments(track_, tr.z, tr.x, cam, width_, height_, projected_, max_y);
    road_.render(*renderer_, track_, projected_, tr.z, tr.x, cam);

    // Player car (screen space, bottom center, offset by lateral)
    float car_x = width_ / 2.f + tr.x * (width_ * 0.3f);
    float car_y = height_ - 60.f;
    // Approximate steer from recent input would be better; use x velocity proxy
    renderer_->draw_player_car(car_x, car_y, tr.x * 0.5f, vel.speed);

    renderer_->draw_hud(vel.speed, tr.z, lap_);

    renderer_->end_frame();
}

} // namespace racer
