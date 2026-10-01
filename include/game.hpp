// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include "background.hpp"
#include "audio.hpp"
#include "components.hpp"
#include "display.hpp"
#include "ecs.hpp"
#include "framebuffer.hpp"
#include "hud.hpp"
#include "input.hpp"
#include "road.hpp"
#include "sprites.hpp"
#include "synth.hpp"
#include "weather.hpp"
#include "track.hpp"

#include <cstdint>
#include <memory>
#include <vector>
#include <string>

namespace racer {

struct ScreenshotOptions {
    std::string path;
    int frames = 0;          // simulation steps at 60 Hz
    float position = 0.f;    // start distance along the track
    int zone = -1;           // if >= 0, start inside this zone instead
    std::string wav_path;    // if set, also write the sound of the run as a WAV
    bool force_steer = false; // replace the autopilot's steering by a constant
    float steer = 0.f;
};

class Game {
public:
    // Native resolution of the software framebuffer.
    static constexpr int width = 320;
    static constexpr int height = 240;
    static constexpr int window_scale = 3;

    // Rear-view mirror: the glass, centred at the top of the screen.
    static constexpr int mirror_width = 112;
    static constexpr int mirror_height = 30;
    static constexpr int mirror_x = (width - mirror_width) / 2;
    static constexpr int mirror_y = 6;

    Game();

    // Interactive mode: opens a window.
    bool init();
    void run();

    // Headless mode: simulates with an autopilot, renders one frame and
    // writes it as a BMP. No window is opened.
    bool screenshot(const ScreenshotOptions& opts);

    // Lists the zones of the track with their start positions on stdout.
    void print_zones() const;

    // Camera position that shows the start of zone `index` once its
    // transition from the previous zone is over.
    float zone_start_position(int index) const;

private:
    void reset();
    void spawn_traffic();
    void update_traffic(float dt);
    void fixed_update(const InputState& input, float dt);
    void update_laps(float prev_z, float z, float dt);
    void show_message(std::string text, float seconds);
    void update_rumble();
    void update_audio(const InputState& input, float dt);
    InputState autopilot() const;
    void render();
    void render_mirror();

    std::unique_ptr<Display> display_;
    Framebuffer fb_;
    Input input_;
    World world_;
    Track track_;
    RoadRenderer road_;
    std::vector<RoadSprite> road_sprites_;
    Framebuffer mirror_fb_;
    RoadRenderer mirror_road_;
    std::vector<RoadSprite> mirror_sprites_;
    SpriteSheet sprites_;
    Background background_;
    Weather weather_;
    Synth synth_;   // declared before audio_, which must be destroyed first
    Audio audio_;
    bool muted_ = false;
    int gear_ = 1;
    float shift_cut_ = 0.f; // seconds left of the throttle lift during an upshift

    Entity player_ = INVALID_ENTITY;
    Entity camera_ = INVALID_ENTITY;
    bool scraping_ = false; // leaning on a rail or cliff
    int scrape_side_ = 1;
    bool crashed_ = false; // a collision happened since the last frame
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

    // Country banner shown when a new zone is entered.
    int zone_ = -1;
    float banner_time_ = 0.f;

    static constexpr float fixed_dt_ = 1.f / 60.f;
};

} // namespace racer
