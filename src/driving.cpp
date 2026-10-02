// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "driving.hpp"

#include "track.hpp"
#include "types.hpp"

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

float Fuel::load(float throttle, float rpm) {
    return 0.15f + 0.85f * std::clamp(throttle, 0.f, 1.f) * (0.5f + 0.5f * std::clamp(rpm, 0.f, 1.f));
}

void Fuel::burn(float load, float dt) {
    level_ = std::max(0.f, level_ - std::clamp(load, 0.f, 1.f) * dt / tank_seconds);
}

void Fuel::refuel(float dt) {
    level_ = std::min(1.f, level_ + dt / fill_seconds);
}

void Fuel::set(float level) {
    level_ = std::clamp(level, 0.f, 1.f);
}

float step_vertical(Vertical& v, float road_y, float road_vy, float gravity, float dt) {
    if (!v.airborne) {
        // Carried on as before, would gravity keep the car on the road?
        const float flight = v.y + v.vy * dt - 0.5f * gravity * dt * dt;
        if (flight <= road_y + takeoff_clearance) {
            // Keep the rate it actually rose at: over a crest that is the
            // climb it just made, which is what launches it.
            v.vy = (road_y - v.y) / dt;
            v.y = road_y;
            return 0.f;
        }
        v.airborne = true;
        v.y = flight;
        v.vy -= gravity * dt;
        return 0.f;
    }
    v.vy -= gravity * dt;
    v.y += v.vy * dt;
    if (v.y > road_y) return 0.f;
    const float impact = std::max(0.f, road_vy - v.vy);
    v.y = road_y;
    v.vy = road_vy;
    v.airborne = false;
    return impact;
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

namespace {

// The hops of a crash: duration in seconds and height in pixels.
constexpr float hop_time[] = {0.6f, 0.5f, 0.4f};
constexpr float hop_height[] = {46.f, 20.f, 8.f};
constexpr float slide_pixels = 28.f;

float smoothstep(float x) {
    x = std::clamp(x, 0.f, 1.f);
    return x * x * (3.f - 2.f * x);
}

} // namespace

CrashPose crash_pose(float t, int side) {
    CrashPose p;
    const float dir = side < 0 ? -1.f : 1.f;
    const float u = std::clamp(t / crash_tumble_seconds, 0.f, 1.f);
    const float ease = 1.f - (1.f - u) * (1.f - u); // fast at first, slowing down
    // Two full rolls, so it ends upright.
    p.angle = t < crash_tumble_seconds ? dir * 4.f * PI * ease : 0.f;
    float start = 0.f;
    for (size_t i = 0; i < 3; ++i) {
        if (t >= start && t < start + hop_time[i]) {
            const float x = (t - start) / hop_time[i];
            p.lift = 4.f * hop_height[i] * x * (1.f - x);
        }
        start += hop_time[i];
    }
    p.recover = smoothstep((t - crash_recover_start) / (crash_seconds - crash_recover_start));
    p.slide = dir * slide_pixels * ease * (1.f - p.recover);
    p.visible = t < crash_recover_start || t >= crash_seconds || static_cast<int>(t / 0.08f) % 2 == 0;
    return p;
}

int crash_landings(float t0, float t1) {
    int n = 0;
    float end = 0.f;
    for (float d : hop_time) {
        end += d;
        if (t0 < end && end <= t1) ++n;
    }
    return n;
}

float follow_speed(float cruise, float leader_speed, float distance, float gap, float closing) {
    return std::clamp(leader_speed + (distance - gap) * closing, 0.f, cruise);
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
