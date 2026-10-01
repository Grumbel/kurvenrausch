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

// Rear view of a car, 96x44 pixels. turn is -1 (left), 0, or +1 (right).
Bitmap make_car(const CarStyle& style, int turn);
// Front view of a car, 96x44 pixels, mirrored as seen in the rear-view mirror.
Bitmap make_car_front(const CarStyle& style);

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
    // steer: -1 left, 0 straight, +1 right
    const Bitmap& player(int steer) const { return player_[static_cast<size_t>(steer + 1)]; }
    const Bitmap& traffic(int style) const {
        return traffic_[static_cast<size_t>(style) % traffic_.size()];
    }
    const Bitmap& traffic_front(int style) const {
        return traffic_front_[static_cast<size_t>(style) % traffic_front_.size()];
    }
    static constexpr int traffic_styles = 4;

private:
    std::array<Bitmap, static_cast<size_t>(Scenery::Count)> scenery_;
    std::array<Bitmap, 3> player_;
    std::array<Bitmap, traffic_styles> traffic_;
    std::array<Bitmap, traffic_styles> traffic_front_;
    Bitmap billboard_back_;

    static bool is_billboard(Scenery kind) {
        return kind == Scenery::Billboard || kind == Scenery::BillboardUs;
    }
};

} // namespace racer
