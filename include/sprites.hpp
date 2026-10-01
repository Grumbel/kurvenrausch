// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include "bitmap.hpp"
#include "track.hpp"

#include <array>

namespace racer {

struct CarStyle {
    Color body_dark;
    Color body;
    Color body_light;
    bool convertible;
};

// Rear view of a car, 96x44 pixels. turn is -1 (left), 0, or +1 (right);
// signal lights the left (-1) or right (+1) indicator; brake the brake lights.
Bitmap make_car(const CarStyle& style, int turn, int signal = 0, bool brake = false);
// Front view of a car, 96x44 pixels, mirrored as seen in the rear-view mirror.
// signal is the side the car turns to, which the mirror shows on that side.
Bitmap make_car_front(const CarStyle& style, int signal = 0);

// The player's convertible with `headroom` empty rows on top, room for a
// raised arm: side -1 the driver waves out on the left, +1 the passenger on
// the right, 0 nobody; frame 0 or 1 animates the wave.
Bitmap make_player_car(const CarStyle& style, int turn, int side, int frame, bool brake, int headroom);

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
    const Bitmap& player(int steer, int wave = 0, int frame = 0, bool brake = false) const {
        const int pose = wave == 0 ? 0 : 1 + (wave > 0 ? 2 : 0) + (frame & 1);
        return player_[static_cast<size_t>(steer + 1)][static_cast<size_t>(pose)][brake ? 1 : 0];
    }
    // signal: indicator blinking towards -1 (left), +1 (right), or 0.
    const Bitmap& traffic(int style, int signal = 0, bool brake = false) const {
        return traffic_[static_cast<size_t>(style) % traffic_styles][static_cast<size_t>(signal + 1)][brake ? 1 : 0];
    }
    const Bitmap& traffic_front(int style, int signal = 0) const {
        return traffic_front_[static_cast<size_t>(style) % traffic_styles][static_cast<size_t>(signal + 1)];
    }
    static constexpr int traffic_styles = 4;

    // Empty rows above the player's car for a waving arm; the car itself is
    // the usual 96x44 below them.
    static constexpr int player_headroom = 12;
    // Exhaust pipe `side` (-1 left, +1 right) of the player's car in its
    // bitmap, for the nitro flames.
    static float exhaust_x(int steer, int side) { return (side < 0 ? 24.f : 72.f) + static_cast<float>(steer); }
    static constexpr float exhaust_y = 38.5f + player_headroom;

private:
    std::array<Bitmap, static_cast<size_t>(Scenery::Count)> scenery_;
    std::array<std::array<std::array<Bitmap, 2>, 5>, 3> player_; // [steer][pose][brake]
    std::array<std::array<std::array<Bitmap, 2>, 3>, traffic_styles> traffic_; // [style][signal][brake]
    std::array<std::array<Bitmap, 3>, traffic_styles> traffic_front_;
    Bitmap billboard_back_;

    static bool is_billboard(Scenery kind) {
        return kind == Scenery::Billboard || kind == Scenery::BillboardUs;
    }
};

} // namespace racer
