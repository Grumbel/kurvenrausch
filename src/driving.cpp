// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "driving.hpp"

#include "track.hpp"

#include <algorithm>
#include <cmath>

namespace racer {

bool Nitro::fire() {
    if (burning() || canisters_ <= 0) return false;
    --canisters_;
    burn_ = burn_seconds;
    return true;
}

void Nitro::update(float dt) {
    burn_ = std::max(0.f, burn_ - dt);
}

float Nitro::intensity() const {
    return std::clamp(burn_ / 0.4f, 0.f, 1.f);
}

float limit_speed(float before, float after, float top, float drag, float dt) {
    if (after <= top) return after;
    // Braking still works up here; accelerating does not.
    return std::max(top, std::min(before, after) - drag * dt);
}

float boosted_speed(float speed, float max_speed) {
    return std::min(speed + pass_boost * max_speed, std::max(speed, pass_boost_limit * max_speed));
}

float signed_gap(float from, float to, float length) {
    float d = std::fmod(to - from, length);
    if (d < 0.f) d += length;
    return d > length / 2.f ? d - length : d;
}

bool close_pass(float gap_before, float gap_after, float lateral, float car_width, float max_step) {
    return gap_before > 0.f && gap_after <= 0.f && gap_before - gap_after < max_step &&
           std::abs(lateral) < 2.f * car_width;
}

int yield_lane(int lanes, float car_x, float player_x, float clearance, const std::vector<bool>& busy) {
    constexpr float centred = 0.1f;
    const float side = car_x - player_x;
    int best = -1;
    float best_cost = 0.f;
    for (int i = 0; i < lanes; ++i) {
        const float lane = lane_center(lanes, i);
        if (std::abs(lane - player_x) < clearance || busy[static_cast<size_t>(i)]) continue;
        // Never cut across the player's line, unless dead ahead in it; even
        // then, rather go to the side the car is already on.
        const bool across = (lane - player_x) * side < 0.f;
        if (across && std::abs(side) > centred) continue;
        const float cost = std::abs(lane - car_x) + (across ? 0.01f : 0.f);
        if (best < 0 || cost < best_cost) {
            best = i;
            best_cost = cost;
        }
    }
    return best;
}

} // namespace racer
