// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <vector>

namespace racer {

// Nitro: a few canisters per lap, each one a burn of a few seconds that adds
// thrust and lifts the top speed.
class Nitro {
public:
    static constexpr int capacity = 3;
    static constexpr float burn_seconds = 3.f;
    static constexpr float top_speed = 1.3f; // top speed while burning, of the normal one
    static constexpr float thrust = 1.5f;    // extra acceleration, of the normal one

    // Starts a burn if none is running and a canister is left.
    bool fire();
    void update(float dt);
    void refill() { canisters_ = capacity; }
    void reset() { canisters_ = capacity; burn_ = 0.f; }

    bool burning() const { return burn_ > 0.f; }
    int canisters() const { return canisters_; }
    // Fraction of the current burn left, 0 when not burning.
    float burn_left() const { return burn_ / burn_seconds; }
    // 0 .. 1, fading out over the last moments of a burn (flames, sound).
    float intensity() const;

private:
    int canisters_ = capacity;
    float burn_ = 0.f;
};

// Speed after one step that changed it from `before` to `after`: the engine
// cannot push the car beyond `top`, and above it (after a nitro burn or a
// boost) the car slows back down by `drag` per second.
float limit_speed(float before, float after, float top, float drag, float dt);

// Passing close by another car gives a little boost: this much of the top
// speed, but not beyond pass_boost_limit times the top speed.
constexpr float pass_boost = 0.08f;
constexpr float pass_boost_limit = 1.1f;
float boosted_speed(float speed, float max_speed);

// Signed distance from `from` to `to` on a looping track of `length`, the
// shorter way round: positive ahead, negative behind.
float signed_gap(float from, float to, float length);

// Has a car that was `gap_before` ahead of the player been passed, with
// `gap_after` the gap now? Only a close pass counts: the cars' centres at most
// two car widths apart (`lateral` and `car_width` in road half-widths). Gaps
// that jump by more than `max_step` are not a pass (respawn, lap seam).
bool close_pass(float gap_before, float gap_after, float lateral, float car_width, float max_step);

// Lane a car pulls over to when honked at: the lane nearest to the car that
// keeps `clearance` from the player's line and is not `busy`, or -1. A car
// never cuts across the player's line, unless it is right in it.
int yield_lane(int lanes, float car_x, float player_x, float clearance, const std::vector<bool>& busy);

} // namespace racer
