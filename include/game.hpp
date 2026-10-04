// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include "background.hpp"
#include "climate.hpp"
#include "daylight.hpp"
#include "animals.hpp"
#include "audio.hpp"
#include "components.hpp"
#include "display.hpp"
#include "driving.hpp"
#include "ecs.hpp"
#include "framebuffer.hpp"
#include "hud.hpp"
#include "input.hpp"
#include "menu.hpp"
#include "police.hpp"
#include "road.hpp"
#include "state.hpp"
#include "sprites.hpp"
#include "synth.hpp"
#include "touch.hpp"
#include "weather.hpp"
#include "track.hpp"
#include "views.hpp"

#include <cstdint>
#include <array>
#include <memory>
#include <optional>
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
    int steer_from = 0;      // ... from this step on
    bool horn = false;       // hold the horn all the way
    int nitro_frame = -1;    // if >= 0, press nitro at this step
    float fuel = -1.f;       // if >= 0, start with this much fuel (0 .. 1)
    int car = -1;            // if >= 0, drive car model `car` (see car_model())
    int visit = -1;          // if >= 0, the autopilot pulls in at the next lot of this kind (a Lot) and stays
    int handbrake_from = -1; // if >= 0, hold the handbrake from this step on
    int brake_from = -1;     // if >= 0, brake (throttle off) from this step on to a stop, let go, then hold it: reverse
    bool pause = false;      // show the pause menu in the screenshot
    int view = 0;            // the camera view, a ViewMode
    float storm = -1.f;      // if >= 0, hold the weather front at this level (0 clear .. 1 storm)
    int music = -1;          // the radio's track in the --wav recording, -1 off
    int police_frame = -1;   // if >= 0, a police chase starts at this step
    std::vector<Finger> touches; // fingers held on the touch screen all run (screen = framebuffer pixels)
    int signal = 0;          // indicators for the headless run: -1 left, +1 right, 2 the hazard lights
    bool headlights = false; // headlights on for the headless run
    float hour = -1.f;       // if >= 0, the time of day the headless run starts at (0 .. 24)
    bool attract = false;    // the headless run shows the attract mode
    float dirt = -1.f;       // if >= 0, start this dirty (mud and oil, 0 .. 1)
    int width = 320;         // the framebuffer's width (320 is 4:3, up to 640)
    int track = 0;           // the track to drive, see track_name()
};

class Game {
public:
    // Native resolution of the software framebuffer: 4:3, or as wide as the
    // screen (up to max_width) when the player chooses the wide screen.
    static constexpr int base_width = 320;
    static constexpr int max_width = 640;
    static constexpr int height = 240;
    static constexpr int window_scale = 3;
    // Horizontal pixels per world unit at scale 1, whatever the width: a
    // wider picture shows more to the sides.
    static constexpr float x_unit = base_width / 2.f;

    // Rear-view mirror: the glass, centred at the top of the screen.
    static constexpr int mirror_width = 112;
    static constexpr int mirror_height = 30;
    static constexpr int mirror_y = 6;

    Game();

    // Interactive mode: opens a window, or covers the screen.
    bool init(bool fullscreen = false);
    // Runs until the player quits; in a web page the browser drives the loop
    // and this returns at once (the Game must then outlive main(): allocate
    // it on the heap and let it be).
    void run();
    // One frame: input, simulation, picture. False once the player quits.
    bool frame();
    // Opens the pause menu, as Start does (the web page calls this when the
    // tab is hidden).
    void pause();

    // Headless mode: simulates with an autopilot, renders one frame and
    // writes it as a BMP. No window is opened.
    bool screenshot(const ScreenshotOptions& opts);

    // Lists the zones of the track with their start positions on stdout.
    void print_zones() const;

    // Drives track `index` (see track_name()) from its start line.
    void load_track(int index);

    // Camera position that shows the start of zone `index` once its
    // transition from the previous zone is over.
    float zone_start_position(int index) const;

private:
    void reset();
    static std::string user_state_dir();
    // Puts the car, standing, at camera position `position`.
    void start_at(float position);
    // Opens and drives the pause menu; false when the player chose to quit.
    bool update_pause(const InputState& input);
    void spawn_traffic();
    void update_traffic(float dt);
    void fixed_update(const InputState& driver_input, float dt);
    // Police chases: one may start, the police car chases, and the chase ends.
    void start_chase();
    void update_police(float dt);
    // The light switches: headlights, indicators, hazard lights.
    void switch_lights(const InputState& input);
    void update_indicators(const InputState& input, float dt);
    // The indicators as the car's sprite shows them now (see hazard_signal).
    int shown_signal() const;
    // The attract mode: following the traffic, waiting for a player.
    void start_attract();
    void leave_attract();
    void follow_next_car();
    void update_attract(float dt);
    // Adds what the fingers press to the input.
    void apply_touch(InputState& input, const std::vector<Finger>& fingers);
    void end_chase();
    void update_laps(float prev_z, float z, float dt);
    void show_message(std::string text, float seconds);
    void update_rumble();
    void update_audio(const InputState& input, float dt);
    void check_close_passes();
    void start_crash(float speed_pct);
    void update_crash(float dt);
    void update_particles(float dt);
    void spawn_dust(float x, float y, int count, float strength);
    void spawn_spray(float speed_pct);
    void spawn_smoke(float speed_pct);
    void land(float impact);
    void apply_car();
    // The gameplay options into effect; `before` were the ones in effect.
    void apply_options(const Options& before);
    // The clock: the day passing, or held at the chosen time.
    void advance_clock(float dt);
    // Where START IN would start for zone `index`: the country, and the
    // region or city where the country has several.
    std::string zone_label(int index) const;
    void change_car();
    // Remembers the car, driver and passenger for the next run.
    void save_choices() const;
    void visit_lot(const InputState& input);
    // The lot whose forecourt the car is on, where it is full width.
    std::optional<Lot> lot_here() const;
    bool parked_at(Lot kind) const { return lot_here() == kind; }
    void update_wash(float dt);
    // Driving the taxi: people hail it at the kerb; stopped beside one with
    // the passenger seat empty, they get in and want to go to a zone a little
    // ahead; stopped there, they pay and get out.
    void update_fares();
    // Now and then an animal of the countryside crosses the road ahead;
    // the horn hurries it, the traffic stops for it, hitting it is a bump or
    // a crash.
    void update_animals(float dt);
    void follow_fork(float prev_car_z);
    int indicator(Entity e, const Transform& t, const Traffic& traffic) const;
    InputState autopilot() const;
    void update_fuel(const InputState& input, float dt);
    void render();
    void render_mirror();
    // The framebuffer to the screen, the touch controls over it.
    void present();
    // The framebuffer this wide (clamped to base_width .. max_width), and the
    // width the screen asks for: 4:3, or as wide as the screen when wide_.
    void set_width(int w);
    int screen_width() const;
    // The touch controls into overlay_, if shown.
    void draw_touch();
    // The track's look at z under the weather passing over now.
    RoadTheme look_at(float z) const {
        return at_daytime(weathered(track_.look_at(z), front_.level()), daylight_at(hour_));
    }
    void update_lightning(float rain, float dt);

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
    bool paused_ = false;
    PauseMenu menu_;
    Options options_;
    OptionsMenu options_menu_;
    bool options_open_ = false; // the pause menu shows the OPTIONS page
    int track_index_ = 0;       // the track driven, see track_name()
    // Where the last run left off (see Choices), taken up by the first race.
    int resume_position_ = -1, resume_minutes_ = -1, resume_tank_ = -1;
    float progress_saved_ = 0.f; // seconds since the position was last saved
    SynthParams sound_;  // what the synth was last told
    bool horn_ = false;
    Nitro nitro_;
    bool nitro_held_ = false; // a burn starts when the button goes down
    float wave_time_ = 0.f;   // seconds left of the wave after a close pass
    int wave_side_ = -1;      // -1 the driver waves (car passed on the left), +1 the passenger
    bool passed_ = false;     // a close pass happened since the last frame (rumble)
    int gear_ = 1;
    float shift_cut_ = 0.f; // seconds left of the throttle lift during an upshift

    Entity player_ = INVALID_ENTITY;
    Entity camera_ = INVALID_ENTITY;
    bool scraping_ = false; // leaning on a rail or cliff
    int scrape_side_ = 1;
    bool crashed_ = false; // a collision happened since the last frame
    int steer_ = 0;     // -1, 0, +1, for the car sprite
    bool braking_ = false; // brake lights
    float wheel_distance_ = 0.f; // distance the tyres have rolled, for their tread frames

    // The player's car (car_model()); the standard top speed, which traffic
    // and the speedometer are measured against whatever the car.
    int car_model_ = 0;
    int driver_ = 0;     // see driver(): the hospital changes it
    int passenger_ = 0;  // see passenger(): the motel changes it
    bool bandaged_ = false;
    ViewMode view_mode_ = ViewMode::Chase;
    bool map_zoomed_ = true; // the mini map shows the stretch around the car, not the whole lap
    float hour_ = start_hour;  // the time of day, 0 .. 24 (see daylight.hpp)
    // The attract mode: the camera follows a car of the traffic, a new one
    // every so often, until somebody presses something.
    bool attract_ = false;
    Entity attract_car_ = INVALID_ENTITY;
    float attract_switch_ = 0.f;  // seconds until the next car
    int attract_cars_ = 0;        // followed so far: the view alternates
    ViewMode played_view_ = ViewMode::Chase; // the player's view, back after the attract mode
    float idle_ = 0.f;            // seconds without any input
    std::vector<uint32_t> day_picture_; // the picture before nightfall, for the headlights' beam
    // The light switches.
    bool headlights_ = false;
    int signal_ = 0;          // the indicator switched on: -1 left, +1 right, 0 none
    bool hazards_ = false;    // the hazard lights: both indicators
    bool beacon_ = false;     // an emergency vehicle's lightbar and siren, on the hazards' switch
    float signal_x_ = 0.f;    // where across the road the car was when it was switched on
    bool blink_on_ = false;   // the indicators lit in this moment of their blinking
    int music_ = 0;          // the radio's track (see Music), -1 off
    Entity police_ = INVALID_ENTITY; // the police car in a chase
    Chase chase_;
    float chase_cooldown_ = chase_cooldown; // seconds until a chase may start
    float pulled_over_ = 0.f; // seconds left standing at the side of the road, caught
    float siren_ = 0.f;       // loudness of the siren, 0 .. 1
    WeatherFront front_;
    float flash_time_ = 0.f;     // seconds left of a lightning flash
    float thunder_delay_ = -1.f; // seconds until its thunder, < 0 for none
    float thunder_strength_ = 0.f;
    float wheel_angle_ = 0.f; // of the steering wheel in the cockpit, radians
    Store store_;         // what is kept between runs; nothing in headless runs
    float record_lap_ = 0.f; // the fastest lap ever, from the store // the driver after a crash, until a hospital patches them up
    Bitmap player_bitmap_; // the car with its people, put together each frame
    float base_max_speed_ = 0.f;
    std::array<std::vector<int>, lot_kinds> lots_; // first full-width forecourt segment of each lot, per kind
    std::optional<Lot> offer_;  // standing at a lot with a choice: it is on offer
    int lot_steer_ = 0;         // the steering last step, to choose once per push
    bool hospital_ambulance_ = false; // at the hospital, the ambulance (after the drivers) is chosen
    struct Hail {
        float z;  // where along the track
        float x;  // at the kerb, road half-widths
        int fare; // who (0 .. fares - 1)
    };
    std::vector<Hail> hails_; // people hailing the taxi
    int fare_zone_ = -1;      // where the fare riding wants to go, -1 none
    int fares_paid_ = 0;
    struct Crossing {
        Animal kind;
        float z, x;          // where; x in road half-widths
        int dir;             // crossing to the right (+1) or the left (-1)
        bool hurried = false;// honked at, or hit: it runs
    };
    std::vector<Crossing> crossings_;
    float crossing_wait_ = animal_min_wait; // seconds until the next one
    std::optional<Lot> autopilot_visit_; // headless: the autopilot visits the next lot of this kind
    Dirt dirt_;
    bool washing_ = false;      // standing in a car wash, being cleaned
    float view_yaw_ = 0.f;   // the view turning over to a newly taken road at a fork,
    float view_shift_ = 0.f; // and moving over to it; see RoadView::yaw and shift

    Fuel fuel_;
    std::vector<MapPoint> map_; // plan view for the mini map
    bool engine_on_ = true;     // false when out of fuel, or for a sputter
    bool refuelling_ = false;
    float stranded_time_ = 0.f; // seconds standing with an empty tank

    // The crash after hitting something solid at speed: seconds since the
    // impact, or negative when not crashing.
    float crash_time_ = -1.f;
    int crash_side_ = 1;       // side of the road it happened on
    float crash_x_ = 0.f;      // lateral position at the impact
    float crash_target_x_ = 0.f; // where the car is put back on the road

    Vertical vertical_;       // the car's height: on the road, or flying off a crest
    float landing_time_ = 0.f; // seconds left of the squash after a hard landing
    bool wet_ = false;        // the tyres are in a wet spot
    bool aquaplaning_ = false; // ... fast enough to lose their grip
    bool oily_ = false;       // sliding on an oil slick
    bool handbraking_ = false; // the handbrake pulled at speed: the rear slides
    bool reverse_armed_ = false; // stopped and the brake let go: pressing it again reverses
    float spin_time_ = 0.f;   // seconds left of the car twitching after an oil slick

    // Debris, dust and spray, in screen space.
    struct Particle {
        enum class Kind : uint8_t { Dust, Debris, Spray };
        float x, y, vx, vy;
        float life, max_life;
        float size;  // radius for dust; debris are single pixels
        Color color;
        Kind kind;
    };
    std::vector<Particle> particles_;
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
    // The interactive loop's state between frames.
    InputState input_state_;
    TouchControls touch_;
    bool touch_seen_ = false;          // the touch screen was used: show its controls
    std::vector<Finger> touch_taps_;   // taps this frame, screen pixels
    Overlay overlay_;                  // drawn over the picture at screen resolution
    int width_ = base_width;           // the framebuffer's width now
    std::vector<uint32_t> car_night_;  // scratch: the car in the dark, kept out of its own headlights
    bool wide_ = false;                // the player chose a picture as wide as the screen
    Uint64 prev_counter_ = 0;
    float accumulator_ = 0.f;
};

} // namespace racer
