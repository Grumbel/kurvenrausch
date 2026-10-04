// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <algorithm>

#include <cmath>
#include <vector>

namespace racer {

// Nitro: a few canisters per lap, each one a burn of a few seconds that adds
// thrust and lifts the top speed.
// The standard car's top speed on the speedometer.
constexpr float top_speed_kmh = 293.f;

class Nitro {
public:
    static constexpr int default_capacity = 3;
    static constexpr float burn_seconds = 3.f;
    static constexpr float top_speed = 1.3f; // top speed while burning, of the normal one
    static constexpr float thrust = 1.5f;    // extra acceleration, of the normal one

    // Starts a burn if none is running and a canister is left.
    bool fire();
    void update(float dt);
    void refill() { canisters_ = capacity_; }
    void reset() { canisters_ = capacity_; burn_ = 0.f; }
    // Canisters each lap (an option); the ones left now stay up to it.
    void set_capacity(int n) { capacity_ = n; canisters_ = std::min(canisters_, n); }
    int capacity() const { return capacity_; }
    void stop() { burn_ = 0.f; } // cuts the current burn short

    bool burning() const { return burn_ > 0.f; }
    int canisters() const { return canisters_; }
    // Fraction of the current burn left, 0 when not burning.
    float burn_left() const { return burn_ / burn_seconds; }
    // 0 .. 1, fading out over the last moments of a burn (flames, sound).
    float intensity() const;

private:
    int capacity_ = default_capacity;
    int canisters_ = default_capacity;
    float burn_ = 0.f;
};

// The fuel tank. Burning depends on the engine's load; at a gas station the
// tank fills up quickly.
class Fuel {
public:
    static constexpr float tank_seconds = 100.f; // a full tank at full load
    static constexpr float fill_seconds = 4.f;   // empty to full at the pump
    static constexpr float low = 0.2f;           // the gauge warns below this
    static constexpr float reserve = 0.3f;       // and a message, once, below this

    // Engine load 0 .. 1 from the pedal and the revs: idling still burns a little.
    static float load(float throttle, float rpm);
    void burn(float load, float dt);
    void refuel(float dt);
    void reset() { level_ = 1.f; }
    void set(float level);

    float level() const { return level_; }
    bool empty() const { return level_ <= 0.f; }
    bool full() const { return level_ >= 1.f; }

private:
    float level_ = 1.f;
};

// How dirty the car is, 0 (clean) .. 1 (filthy) for each kind of dirt: mud
// from ploughing through puddles and from crashes, oil from slicks. The car
// wash takes it off quickly, rain slowly (and oil hardly at all).
class Dirt {
public:
    static constexpr float puddle_rate = 0.5f;  // mud per second through a puddle at top speed
    static constexpr float oil_rate = 0.6f;     // oil per second on a slick
    static constexpr float crash_mud = 0.4f;    // mud from a crash at top speed
    static constexpr float wash_seconds = 3.f;  // filthy to clean in the car wash
    static constexpr float rinse_seconds = 60.f; // mud to clean in a downpour; oil takes four times as long

    // Through a puddle at speed_pct (0 .. 1) of top speed for dt seconds.
    void splash(float speed_pct, float dt);
    void oil(float dt);
    // A crash at speed_pct of top speed.
    void crash(float speed_pct);
    void wash(float dt);
    // Rain of `rain` (0 .. 1) rinsing the car for dt seconds.
    void rinse(float rain, float dt);
    void set(float mud, float oil);
    void reset() { set(0.f, 0.f); }

    float mud() const { return mud_; }
    float oil() const { return oil_; }
    bool clean() const { return mud_ <= 0.f && oil_ <= 0.f; }

private:
    float mud_ = 0.f, oil_ = 0.f;
};

// The car's height over crests: it follows the road unless the road falls
// away faster than gravity can pull it down, then it flies until it lands.
// World units; `gravity` in units per second squared.
constexpr float jump_gravity = 40000.f;
struct Vertical {
    float y = 0.f;       // height of the car
    float vy = 0.f;      // its vertical speed, upwards positive
    bool airborne = false;
};
// One step: `road_y` is the road's height under the car now and `road_vy` the
// rate the car rises with it while driving on it (slope times speed). Returns
// the impact speed (how much faster the car falls than the road there) on
// the step it lands, otherwise 0.
// After a jump in position (a restart, being put back after a crash) put the
// car on the road with place_on_road() first, or the jump in height would
// count as a climb.
float step_vertical(Vertical& v, float road_y, float road_vy, float gravity, float dt);
inline void place_on_road(Vertical& v, float road_y) { v = Vertical{road_y, 0.f, false}; }
// It only takes off when it would clear the road by this much, so a sharp
// change of grade taken slowly is not a hop.
constexpr float takeoff_clearance = 15.f;

// At a fork, is a car at lateral position x (relative to the road it is
// on) nearer to the other road, whose centre is at `other`, by a clear
// margin (road half-widths)? Then it is on that one now. The margin keeps it
// from flipping back and forth while the roads still coincide.
constexpr float fork_switch_margin = 0.1f;
inline bool nearer_other_road(float x, float other) {
    return std::abs(x - other) + fork_switch_margin < std::abs(x);
}

// How fast the car moves sideways under full steering, in road half-widths
// per second, at `speed` (a fraction of the top speed). From half speed up
// it grows with the speed; below that it falls off only like a square root,
// so the car still manoeuvres when slow (pulling onto a forecourt, say),
// down to nothing when standing.
float steer_rate(float speed);

// Reversing. Once the car has come to a full stop and the brake has been let
// go, the reverse gear is ready (`armed`, kept by reverse_armed() step by
// step; moving forwards again disarms it). Then pressing the brake again,
// without the throttle, drives the car backwards, up to `max_reverse`; the
// throttle brakes a car rolling backwards to a stop (`stop_decel`), letting
// go of both lets it roll to a stop more gently. Speeds are negative
// backwards; all magnitudes are positive.
bool reverse_armed(bool armed, float speed, float throttle, float brake);
bool in_reverse(float speed, float throttle, float brake, bool armed);
float reverse_speed(float speed, float throttle, float brake, float accel, float stop_decel, float max_reverse,
                    float dt);

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

// Speed a car wants when following a vehicle going `leader_speed` at
// `distance` ahead: the leader's speed at the gap it keeps, a bit faster
// (`closing` per unit of distance) further back to close in, slower when
// too close to open the gap again; never above `cruise`, never backwards.
float follow_speed(float cruise, float leader_speed, float distance, float gap, float closing);

// The crash when the car hits something solid at speed, as a function of the
// time since the impact: the car tumbles through a few decaying hops while
// rolling over twice, slides outwards and comes to rest upright, then is put
// back on the road, blinking.
struct CrashPose {
    float angle = 0.f;   // roll in radians, clockwise on screen
    float lift = 0.f;    // pixels above the ground
    float slide = 0.f;   // pixels sideways, towards the crash side
    float recover = 0.f; // 0 .. 1: progress of being put back on the road
    bool visible = true; // blinks while being put back
};
constexpr float crash_tumble_seconds = 1.5f;
constexpr float crash_recover_start = 2.0f;
constexpr float crash_seconds = 2.8f;
// side: -1 crashed on the left of the road, +1 on the right.
CrashPose crash_pose(float t, int side);
// Number of times the car touched down in (t0, t1].
int crash_landings(float t0, float t1);

} // namespace racer
