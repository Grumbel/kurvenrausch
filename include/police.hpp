// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

namespace racer {

// Police chases. Now and then, driving fast, a police car turns up behind,
// lights flashing, siren wailing. It closes in, overtakes in the next lane,
// cuts in ahead and slows down to stop the car: brought to a halt behind
// it, the car is pulled over. Swerve past it and the chase goes on; get far
// enough ahead, or hold out until it gives up, and the car has escaped.
// Gaps are the player's distance ahead of the police car, in segments
// (negative once the police car is ahead); lateral positions in road
// half-widths.
constexpr float chase_rate = 1.f / 75.f;   // chases per second while driving fast
constexpr float chase_min_speed = 0.8f;    // ... at this fraction of top speed or more
constexpr float chase_cooldown = 60.f;     // seconds without one after a chase, and at the start
constexpr float chase_start_gap = 45.f;    // where it turns up behind
constexpr float chase_escape_gap = 90.f;   // this far ahead, it is lost
constexpr float chase_give_up = 40.f;      // seconds it keeps going, then it drops back
constexpr float chase_overtake_gap = 8.f;  // this close behind, it pulls out to overtake
constexpr float chase_block_gap = 4.f;     // this far ahead, it cuts in to block
constexpr float chase_lane_offset = 0.7f;  // how far beside the car it overtakes
constexpr float chase_stop_speed = 0.05f;  // the car counts as stopped below this (of top speed)
constexpr float chase_stop_seconds = 1.f;  // ... behind the police car for this long: caught
constexpr float pulled_over_seconds = 4.f; // then it waits, and the police drive off
constexpr float police_leave_gap = 60.f;   // driven off this far, the police car is gone

enum class ChasePhase {
    Closing,    // coming up from behind
    Overtaking, // passing in the next lane
    Blocking,   // ahead in the car's lane, slowing to a stop
    Leaving,    // done (caught): driving off
};

enum class ChaseOutcome { Going, Escaped, Caught };

struct Chase {
    float time = 0.f;       // seconds since it started
    ChasePhase phase = ChasePhase::Closing;
    float stopped = 0.f;    // seconds the car has stood behind the blocking police car
    bool giving_up = false; // out of time: it slows down, siren off, and drops back
};

// Where the police car heads and how fast, for its phase: closing in flat
// out (as fast as the standard car, `top`, faster when far behind); passing
// beside the car, faster than it; ahead, in the car's lane and braking;
// leaving or giving up, on its way.
struct PoliceMove {
    float speed;
    float x;
};
PoliceMove police_move(const Chase& chase, float police_speed, float player_speed, float player_x,
                       float gap, float top, float dt);

// One step of the chase: the phase moves on with the gap and the car's speed
// (of top) and lateral distance to the police car; the result says who won.
ChaseOutcome update_chase(Chase& chase, float gap, float player_speed_pct, float lateral, float dt);

// Does a chase start this step? `roll` uniform in [0, 1).
bool chase_starts(float roll, float speed_pct, float dt);

} // namespace racer
