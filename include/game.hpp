#pragma once
#include "ecs.hpp"
#include "components.hpp"
#include "renderer.hpp"
#include "road.hpp"
#include "input.hpp"
#include <memory>

namespace racer {

class Game {
public:
    Game(int width = 1024, int height = 768);
    ~Game();

    bool init();
    void run();
    void shutdown();

private:
    void update(float dt);
    void render();
    void fixed_update(float dt);

    int width_, height_;
    SDL_Window* window_ = nullptr;
    std::unique_ptr<Renderer> renderer_;
    std::unique_ptr<Input> input_;
    World world_;
    Track track_;
    RoadSystem road_;
    std::vector<Projected> projected_;

    Entity player_ = INVALID_ENTITY;
    Entity camera_ = INVALID_ENTITY;

    float time_ = 0.f;
    float accumulator_ = 0.f;
    const float fixed_dt_ = 1.f / 60.f;
    bool running_ = true;
    int lap_ = 1;
};

} // namespace racer
