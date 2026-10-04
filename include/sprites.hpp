// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include "bitmap.hpp"
#include "track.hpp"
#include "people.hpp"
#include "vehicles.hpp"

#include <array>
#include <cmath>
#include <map>
#include <vector>

namespace racer {

// The `signal` of the vehicle sprites: -1 the left indicator lit, +1 the
// right one, 0 neither, hazard_signal both (the hazard lights).
constexpr int hazard_signal = 2;

struct CarStyle {
    Color body_dark;
    Color body;
    Color body_light;
    bool convertible;
};

// Dirties a car's bitmap in place: mud (0 .. 1) in splotches, thickest low
// down, oil (0 .. 1) in black spots. The outline stays.
void apply_dirt(Bitmap& car, float mud, float oil);

// Rear view of a car, 96x44 pixels. turn is -1 (left), 0, or +1 (right);
// signal lights the left (-1) or right (+1) indicator; brake the brake lights;
// tread_frame (0 .. 2) shifts the tyres' tread, for rolling.
// people: a convertible shows its driver and passenger (traffic); without,
// they are drawn over it by make_occupants() (the player's car).
Bitmap make_car(const CarStyle& style, int turn, int signal = 0, bool brake = false, int tread_frame = 0,
                bool people = true);
// Front view of a car, 96x44 pixels, mirrored as seen in the rear-view mirror.
// signal is the side the car turns to, which the mirror shows on that side.
Bitmap make_car_front(const CarStyle& style, int signal = 0, int tread_frame = 0);

// The other vehicles, at the same 6.25 world units per pixel as the cars:
// rear views with indicator and brake light variants, and front views for
// the mirror, like make_car() and make_car_front().
Bitmap make_van(const CarStyle& style, int signal, bool brake, int tread = 0);      // 104x64
Bitmap make_van_front(const CarStyle& style, int signal, int tread = 0);
// An emergency lightbar, red on the left and blue on the right, centred on
// column cx with its top at row y: `lit` -1 flashes the red, +1 the blue,
// 0 neither.
void paint_lightbar(Bitmap& b, int lit, int cx, int y);
// Someone standing at the kerb, facing the road, hailing a taxi with a
// raised arm (two frames), 28x92.
Bitmap make_pedestrian(const Person& person, int frame);
// An ambulance: the panel van in white, a red stripe and crosses, its
// lightbar `lights` (as paint_lightbar()) on the roof.
Bitmap make_ambulance(const CarStyle& style, int signal, int lights, bool brake, int tread = 0); // 104x64
Bitmap make_ambulance_front(const CarStyle& style, int lights, int tread = 0);
// Where its lightbar sits.
constexpr int ambulance_lightbar_x = 52, ambulance_lightbar_y = 1;
Bitmap make_hatch(const CarStyle& style, int signal, bool brake, int tread = 0);    // 86x46
Bitmap make_hatch_front(const CarStyle& style, int signal, int tread = 0);
Bitmap make_pickup(const CarStyle& style, int signal, bool brake, int tread = 0);   // 102x58
Bitmap make_pickup_front(const CarStyle& style, int signal, int tread = 0);
Bitmap make_bus(const CarStyle& style, int signal, bool brake, int tread = 0);      // 115x116
Bitmap make_bus_front(const CarStyle& style, int signal, int tread = 0);
Bitmap make_truck(const CarStyle& style, int signal, bool brake, int tread = 0);    // 112x104
Bitmap make_truck_front(const CarStyle& style, int signal, int tread = 0);
Bitmap make_rival(const CarStyle& style, int signal, bool brake, int tread = 0);    // 100x40
Bitmap make_rival_front(const CarStyle& style, int signal, int tread = 0);

// The player's car, without the people, with `headroom` empty rows on top
// for a raised arm.
Bitmap make_player_car(const CarStyle& style, int turn, bool brake, int signal, int tread, int headroom);

// A police car, from behind and from the front (as in the mirror): `lights`
// -1 flashes the red half of the lightbar, +1 the blue, 0 neither.
Bitmap make_police(const CarStyle& style, int lights, bool brake, int tread);
Bitmap make_police_front(const CarStyle& style, int lights, int tread);

// The player's police car (lightbar off) and the taxi's roof sign, drawn
// into the headroom of a player car sprite leaning by `lean` pixels.
Bitmap make_player_police(const CarStyle& style, int turn, bool brake, int signal, int tread, int headroom);
void taxi_sign(Bitmap& b, int lean, int headroom);

// The player's truck from behind, the same size as make_player_car(): a
// cab-over tractor whose cab fills the headroom, its rear window where a
// car's is, so make_occupants() fits it as a closed car.
Bitmap make_player_truck(const CarStyle& style, int turn, bool brake, int signal, int tread, int headroom);

// The cockpit view's dashboard, `width` pixels wide and dashboard_height
// high: the car's colour along the base of the windscreen, the dash with
// the instruments behind the wheel on the left, the vents in the middle.
constexpr int dashboard_height = 60;
constexpr int dashboard_wheel_x = 124; // centre of the steering wheel
Bitmap make_dashboard(const CarStyle& style, int width);
// The steering wheel, wheel_size square, with the driver's hands on it at
// ten to two, to draw rotated.
constexpr int wheel_size = 92;
Bitmap make_wheel(const Person& driver);

// The people in the player's car, to draw over make_player_car() (same size):
// in a convertible their heads above the seats, in a closed car dimly
// through the rear window. wave: -1 the driver waves out on the left, +1 the
// passenger on the right, 0 nobody; frame 0 or 1 animates the wave.
// bandaged: the driver wears a bandage round the head, after a crash.
Bitmap make_occupants(const Person& driver, const Person& passenger, int turn, int wave, int frame, bool convertible,
                      int headroom, bool bandaged = false);

// The application icon, 32x32 pixel art: a road into a sunset with the red
// car on it, on a rounded square. The window icon and the desktop icons
// (tools/make_icons.py, via --icon) all come from this.
Bitmap make_app_icon();

// The application's ID, as used for the desktop entry, the icons and the
// AppStream metadata (reverse DNS of the project's home).
constexpr const char* app_id = "io.github.grumbel.kurvenrausch";

// All game sprites, generated procedurally as pixel art at startup.
class SpriteSheet {
public:
    SpriteSheet();

    const Bitmap& scenery(Scenery kind) const { return scenery_[static_cast<size_t>(kind)]; }
    // Scenery seen from behind (in the mirror): the back of billboards,
    // otherwise the same as scenery().
    const Bitmap& scenery_back(Scenery kind) const {
        return is_billboard(kind) ? billboard_back_ : scenery(kind);
    }
    // The player's car, without its people: model (see car_model()); steer
    // -1 left, 0 straight, +1 right; brake lights; tyre frame.
    // signal: the indicators lit, -1 left, +1 right, hazard_signal both.
    const Bitmap& player(int model, int steer, bool brake = false, int tread = 0, int signal = 0) const {
        const auto m = static_cast<size_t>(((model % car_models) + car_models) % car_models);
        return player_[m][static_cast<size_t>(steer + 1)][brake ? 1 : 0][static_cast<size_t>(signal + 1)]
                      [tread_index(tread)];
    }
    bool player_convertible(int model) const {
        return player_convertible_[static_cast<size_t>(((model % car_models) + car_models) % car_models)];
    }
    // The cockpit: the dashboard of a model and the wheel in a driver's hands.
    const Bitmap& dashboard(int model) const {
        return dashboards_[static_cast<size_t>(((model % car_models) + car_models) % car_models)];
    }
    const Bitmap& wheel(int driver_index) const {
        return wheels_[static_cast<size_t>(((driver_index % drivers) + drivers) % drivers)];
    }
    // A fare (0 .. fares - 1) hailing a taxi at the kerb, frame 0 or 1.
    const Bitmap& pedestrian(int fare, int frame) const {
        return pedestrians_[static_cast<size_t>(((fare % fares) + fares) % fares)][static_cast<size_t>(frame & 1)];
    }
    // The people to draw over it (see make_occupants()), made once and kept.
    const Bitmap& occupants(int driver_index, int passenger_index, int steer, int wave, int frame, int model,
                            bool bandaged = false) const;
    // A vehicle in the traffic from behind; style picks the colours (modulo
    // vehicle_info(kind).styles), signal the indicator blinking towards -1
    // (left), +1 (right), or 0.
    const Bitmap& vehicle(Vehicle kind, int style, int signal = 0, bool brake = false, int tread = 0) const {
        const auto& styles = vehicles_[static_cast<size_t>(kind)];
        return styles[static_cast<size_t>(style) % styles.size()]
            .rear[static_cast<size_t>(signal + 1)][brake ? 1 : 0][tread_index(tread)];
    }
    // The same vehicle from the front, as seen in the mirror.
    const Bitmap& vehicle_front(Vehicle kind, int style, int signal = 0, int tread = 0) const {
        const auto& styles = vehicles_[static_cast<size_t>(kind)];
        return styles[static_cast<size_t>(style) % styles.size()].front[static_cast<size_t>(signal + 1)][tread_index(tread)];
    }

    // Tyre tread frames: the tread rows shift a pixel per frame. Picked by
    // the distance driven, the tyres roll visibly when slow and flicker at
    // speed, where a frame step passes in less than a tick.
    static constexpr int tyre_frames = 3;
    static constexpr float tread_step = 25.f; // world units driven per frame
    static int tyre_frame(float distance) {
        const int f = static_cast<int>(std::floor(distance / tread_step)) % tyre_frames;
        return f < 0 ? f + tyre_frames : f;
    }

    // Empty rows above the player's car for a waving arm; the car itself is
    // the usual 96x44 below them.
    static constexpr int player_headroom = 12;
    // Exhaust pipe `side` (-1 left, +1 right) of the player's car in its
    // bitmap, for the nitro flames.
    static float exhaust_x(int steer, int side) { return (side < 0 ? 24.f : 72.f) + static_cast<float>(steer); }
    static constexpr float exhaust_y = 38.5f + player_headroom;

private:
    std::array<Bitmap, static_cast<size_t>(Scenery::Count)> scenery_;
    static size_t tread_index(int tread) { return static_cast<size_t>(((tread % tyre_frames) + tyre_frames) % tyre_frames); }

    // [model][steer][brake][tread]
    // [model][steer][brake][signal + 1][tread]
    std::array<std::array<std::array<std::array<std::array<Bitmap, tyre_frames>, 4>, 2>, 3>, car_models> player_;
    std::array<bool, car_models> player_convertible_{};
    std::array<Bitmap, car_models> dashboards_;
    std::array<std::array<Bitmap, 2>, fares> pedestrians_;
    std::array<Bitmap, drivers> wheels_;
    mutable std::map<uint32_t, Bitmap> occupants_;
    struct VehicleSprites {
        std::array<std::array<std::array<Bitmap, tyre_frames>, 2>, 3> rear; // [signal][brake][tread]
        std::array<std::array<Bitmap, tyre_frames>, 3> front;               // [signal][tread]
    };
    std::array<std::vector<VehicleSprites>, static_cast<size_t>(Vehicle::Count)> vehicles_; // [kind][style]
    Bitmap billboard_back_;

    static bool is_billboard(Scenery kind) {
        return kind == Scenery::Billboard || kind == Scenery::BillboardUs;
    }
};

} // namespace racer
