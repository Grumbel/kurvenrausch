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

// All game sprites, generated procedurally as pixel art at startup.
class SpriteSheet {
public:
    SpriteSheet();

    const Bitmap& scenery(Scenery kind) const { return scenery_[static_cast<size_t>(kind)]; }
    // steer: -1 left, 0 straight, +1 right
    const Bitmap& player(int steer) const { return player_[static_cast<size_t>(steer + 1)]; }
    const Bitmap& traffic(int style) const {
        return traffic_[static_cast<size_t>(style) % traffic_.size()];
    }
    static constexpr int traffic_styles = 4;

private:
    std::array<Bitmap, static_cast<size_t>(Scenery::Count)> scenery_;
    std::array<Bitmap, 3> player_;
    std::array<Bitmap, traffic_styles> traffic_;
};

} // namespace racer
