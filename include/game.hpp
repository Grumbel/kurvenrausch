#pragma once
#include "background.hpp"
#include "components.hpp"
#include "display.hpp"
#include "ecs.hpp"
#include "framebuffer.hpp"
#include "hud.hpp"
#include "input.hpp"
#include "road.hpp"
#include "sprites.hpp"
#include "track.hpp"

#include <cstdint>
#include <memory>
#include <string>

namespace racer {

struct ScreenshotOptions {
    std::string path;
    int frames = 0;          // simulation steps at 60 Hz
    float position = 0.f;    // start distance along the track
};

class Game {
public:
    // Native resolution of the software framebuffer.
    static constexpr int width = 320;
    static constexpr int height = 240;
    static constexpr int window_scale = 3;

    Game();

    // Interactive mode: opens a window.
    bool init();
    void run();

    // Headless mode: simulates with an autopilot, renders one frame and
    // writes it as a BMP. No window is opened.
    bool screenshot(const ScreenshotOptions& opts);

private:
    void reset();
    void fixed_update(const InputState& input, float dt);
    void update_laps(float prev_z, float z, float dt);
    void show_message(std::string text, float seconds);
    InputState autopilot() const;
    void render();

    std::unique_ptr<Display> display_;
    Framebuffer fb_;
    Input input_;
    World world_;
    Track track_;
    RoadRenderer road_;
    SpriteSheet sprites_;
    Background background_;

    Entity player_ = INVALID_ENTITY;
    Entity camera_ = INVALID_ENTITY;
    int steer_ = 0;     // -1, 0, +1, for the car sprite
    float bounce_ = 0.f; // vertical shake of the car in pixels
    uint32_t rng_ = 0x2545f491u;

    // Lap timing: the clock starts when the start line is first crossed.
    bool race_started_ = false;
    int lap_ = 0;
    float lap_time_ = 0.f;
    float last_lap_ = 0.f;
    float best_lap_ = 0.f;

    // Banner message (lap completed, new record), shown for a while.
    std::string message_;
    float message_time_ = 0.f;
    float clock_ = 0.f;

    static constexpr float fixed_dt_ = 1.f / 60.f;
};

} // namespace racer
