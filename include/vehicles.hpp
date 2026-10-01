// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>

namespace racer {

// The kinds of vehicle in the traffic.
enum class Vehicle : uint8_t {
    Car,
    Van,
    Truck,  // a box trailer, slow
    Rival,  // a rare sports car that races the player
    Count
};

struct VehicleInfo {
    float width;      // world units
    float min_speed;  // cruising speed range, fractions of the player's top speed
    float max_speed;
    int styles;       // colour schemes in the sprite sheet
    float share;      // fraction of the traffic
};

const VehicleInfo& vehicle_info(Vehicle kind);

// Picks a vehicle kind by the shares, for `roll` uniform in [0, 1).
Vehicle traffic_vehicle(float roll);

// A rival cruises at `cruise` until the player comes close, ahead or just
// behind within `race_range` segments; then it races at rival_race_speed
// (of the top speed `max_speed`). `gap` is the player's distance ahead of
// the rival, in segments.
constexpr float rival_race_speed = 0.97f;
constexpr float rival_race_range = 30.f;
float rival_speed(float cruise, float max_speed, float gap);

} // namespace racer
