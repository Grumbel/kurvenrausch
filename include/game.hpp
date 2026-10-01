#pragma once
#include "components.hpp"
#include "display.hpp"
#include "ecs.hpp"
#include "input.hpp"
#include "renderer.hpp"
#include "road.hpp"

#include <memory>
#include <string>

namespace racer {

class Game {
public:
    Game(int width, int height);

    // Interactive mode: opens a window.
    bool init();
    void run();

    // Headless mode: simulates `frames` fixed steps with the throttle held,
    // renders one frame and writes it to `path`. No window is opened.
    bool screenshot(const std::string& path, int frames);

private:
    void setup_world();
    void reset();
    void fixed_update(const InputState& input, float dt);
    void render();

    int width_, height_;
    std::unique_ptr<Display> display_;
    Renderer renderer_;
    Input input_;
    World world_;
    Track track_;
    RoadSystem road_;
    std::vector<Projected> projected_;

    Entity player_ = INVALID_ENTITY;
    Entity camera_ = INVALID_ENTITY;

    const float fixed_dt_ = 1.f / 60.f;
    int lap_ = 1;
};

} // namespace racer
