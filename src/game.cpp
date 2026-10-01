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
        "Kurvenrausch – Classic Pseudo-3D",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        width_, height_,
        SDL_WINDOW_SHOWN
    );
    if (!window_) {
        std::cerr << "Window creation failed: " << SDL_GetError() << "\n";
        return false;
    }

    renderer_ = std::make_unique<Renderer>(width_, height_);
    if (!renderer_->init(window_)) {
        std::cerr << "Renderer init failed: " << SDL_GetError() << "\n";
        return false;
    }

    input_ = std::make_unique<Input>();
    road_.build_demo_track(track_);

    player_ = world_.create();
    world_.add<Transform>(player_, Transform{0.f, 0.f, 0.f});
    world_.add<Velocity>(player_, Velocity{0.f});
    world_.add<Player>(player_);

    camera_ = world_.create();
    Camera cam;
    cam.height = 1000.f;
    cam.depth  = 0.84f;
    cam.draw_distance = 150.f;   // segments
    world_.add<Camera>(camera_, cam);
    world_.add<Transform>(camera_, Transform{});

    projected_.reserve(static_cast<size_t>(cam.draw_distance) + 2);

    std::cout << "Kurvenrausch ready.\n"
              << "  Arrows / WASD  drive\n"
              << "  R             restart\n"
              << "  Esc           quit\n"
              << "Track: " << track_.segments.size() << " segments, "
              << track_.total_length << " units\n";
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
        if (dt > 0.05f) dt = 0.05f;

        InputState state;
        input_->poll(state);
        if (state.quit) running_ = false;

        auto& tr  = world_.get<Transform>(player_);
        auto& vel = world_.get<Velocity>(player_);
        auto& pl  = world_.get<Player>(player_);

        if (state.restart) {
            tr.z = 0.f; tr.x = 0.f;
            vel.speed = 0.f;
            lap_ = 1;
        }

        // Speed
        if (state.up)        vel.speed += pl.accel * dt;
        else if (state.down) vel.speed -= pl.brake * dt;
        else                 vel.speed -= 40.f * dt;   // drag
        vel.speed = clamp(vel.speed, 0.f, pl.max_speed);

        // Steering + centrifugal
        float steer = 0.f;
        if (state.left)  steer -= 1.f;
        if (state.right) steer += 1.f;

        int seg_idx = track_.index_from_z(tr.z);
        float curve = track_.get(seg_idx).curve;
        float centrifugal = curve * (vel.speed / pl.max_speed) * pl.centrifugal;

        float speed_factor = vel.speed / pl.max_speed;
        tr.x += (steer * pl.steer_speed - centrifugal) * speed_factor * dt * 2.5f;
        tr.x = clamp(tr.x, -1.8f, 1.8f);

        // Off-road penalty
        if (std::abs(tr.x) > 1.05f)
            vel.speed *= (1.f - 1.5f * dt);

        tr.z += vel.speed * dt;
        if (tr.z >= track_.total_length) {
            tr.z -= track_.total_length;
            ++lap_;
        }

        // Camera follows player
        auto& cam_tr = world_.get<Transform>(camera_);
        cam_tr.z = tr.z;
        cam_tr.x = tr.x;
        cam_tr.y = track_.get(seg_idx).y;

        render();
        time_ += dt;
    }
}

void Game::fixed_update(float) {}
void Game::update(float) {}

void Game::render() {
    renderer_->begin_frame();

    auto& cam = world_.get<Camera>(camera_);
    auto& tr  = world_.get<Transform>(player_);
    auto& vel = world_.get<Velocity>(player_);

    renderer_->draw_background(tr.x * 40.f, 0.f);

    float max_y = 0.f;
    road_.project_segments(track_, tr.z, tr.x, cam,
                           width_, height_, projected_, max_y);
    road_.render(*renderer_, track_, projected_, tr.z, tr.x, cam);

    // Player car fixed near bottom of screen, shifted by lateral position
    float car_x = width_ / 2.f + tr.x * (width_ * 0.35f);
    float car_y = height_ - 50.f;
    renderer_->draw_player_car(car_x, car_y, tr.x, vel.speed);

    renderer_->draw_hud(vel.speed, tr.z, lap_);
    renderer_->end_frame();
}

} // namespace racer
