// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include "bitmap.hpp"
#include "track.hpp"
#include "vehicles.hpp"

#include <array>
#include <cmath>
#include <vector>

namespace racer {

struct CarStyle {
    Color body_dark;
    Color body;
    Color body_light;
    bool convertible;
};

// Rear view of a car, 96x44 pixels. turn is -1 (left), 0, or +1 (right);
// signal lights the left (-1) or right (+1) indicator; brake the brake lights;
// tread_frame (0 .. 2) shifts the tyres' tread, for rolling.
Bitmap make_car(const CarStyle& style, int turn, int signal = 0, bool brake = false, int tread_frame = 0);
// Front view of a car, 96x44 pixels, mirrored as seen in the rear-view mirror.
// signal is the side the car turns to, which the mirror shows on that side.
Bitmap make_car_front(const CarStyle& style, int signal = 0, int tread_frame = 0);

// The other vehicles, at the same 6.25 world units per pixel as the cars:
// rear views with indicator and brake light variants, and front views for
// the mirror, like make_car() and make_car_front().
Bitmap make_van(const CarStyle& style, int signal, bool brake, int tread = 0);      // 104x64
Bitmap make_van_front(const CarStyle& style, int signal, int tread = 0);
Bitmap make_truck(const CarStyle& style, int signal, bool brake, int tread = 0);    // 112x104
Bitmap make_truck_front(const CarStyle& style, int signal, int tread = 0);
Bitmap make_rival(const CarStyle& style, int signal, bool brake, int tread = 0);    // 100x40
Bitmap make_rival_front(const CarStyle& style, int signal, int tread = 0);

// The player's convertible with `headroom` empty rows on top, room for a
// raised arm: side -1 the driver waves out on the left, +1 the passenger on
// the right, 0 nobody; frame 0 or 1 animates the wave.
Bitmap make_player_car(const CarStyle& style, int turn, int side, int frame, bool brake, int tread, int headroom);

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
    // steer: -1 left, 0 straight, +1 right. wave: -1 the driver waves on the
    // left, +1 the passenger on the right, 0 nobody; frame 0 or 1. brake
    // lights the brake lights.
    const Bitmap& player(int steer, int wave = 0, int frame = 0, bool brake = false, int tread = 0) const {
        const int pose = wave == 0 ? 0 : 1 + (wave > 0 ? 2 : 0) + (frame & 1);
        return player_[static_cast<size_t>(steer + 1)][static_cast<size_t>(pose)][brake ? 1 : 0][tread_index(tread)];
    }
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

    std::array<std::array<std::array<std::array<Bitmap, tyre_frames>, 2>, 5>, 3> player_; // [steer][pose][brake][tread]
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
