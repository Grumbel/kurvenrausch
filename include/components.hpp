// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include <cmath>

namespace racer {

// Position on the track: x is the lateral offset in road half-widths, z the
// distance along the track. For the player, z is the camera position (the car
// itself is player_z ahead of it) and y the road height under the car; for
// traffic, z is the car's own position.
struct Transform {
    float x = 0.f;
    float y = 0.f;
    float z = 0.f;
};

struct Velocity {
    float speed = 0.f; // world units per second along the track
};

// Driving characteristics. Speeds are in world units per second.
struct Player {
    float max_speed = 0.f;
    float accel = 0.f;
    float brake = 0.f;           // negative
    float decel = 0.f;           // negative, when coasting
    float offroad_decel = 0.f;   // negative
    float offroad_limit = 0.f;   // off-road slowdown only above this speed
    float centrifugal = 0.3f;    // outward push in curves
    float car_width = 600.f;     // world units

    // Classic tuning: top speed covers one segment per 60 Hz tick.
    static Player for_segment_length(float segment_length) {
        Player p;
        p.max_speed = segment_length * 60.f;
        p.accel = p.max_speed / 5.f;
        p.brake = -p.max_speed;
        p.decel = -p.max_speed / 5.f;
        p.offroad_decel = -p.max_speed / 2.f;
        p.offroad_limit = p.max_speed / 4.f;
        return p;
    }
};

// AI traffic: keeps to a lane, changes lanes to pass slower vehicles and
// pulls over when the player honks.
struct Traffic {
    int style = 0;          // sprite variant
    float target_x = 0.f;   // lateral position of the lane it is heading for
    float gap = 0.f;        // distance ahead of the player's car at the last step
    float startled = 0.f;   // seconds of hurried swerving left after a honk
};

struct Camera {
    float height = 1000.f;      // above the road surface
    float depth = 1.f / std::tan(50.f * 3.14159265f / 180.f); // 100 deg FOV
    int draw_distance = 300;    // segments

    // Distance from the camera to the player's car.
    float player_z() const { return height * depth; }
};

} // namespace racer
