// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

namespace racer {

// Police chases. Now and then, driving fast, a police car turns up behind,
// lights flashing, siren wailing. Pull far enough ahead or hold out long
// enough and it gives up; let it tail you closely for long enough and you
// are pulled over. Gaps are the player's distance ahead of the police car,
// in segments.
constexpr float chase_rate = 1.f / 75.f;   // chases per second while driving fast
constexpr float chase_min_speed = 0.8f;    // ... at this fraction of top speed or more
constexpr float chase_cooldown = 60.f;     // seconds without one after a chase, and at the start
constexpr float chase_start_gap = 45.f;    // where it turns up behind
constexpr float chase_escape_gap = 90.f;   // this far ahead, it is lost
constexpr float chase_give_up = 40.f;      // seconds it keeps going, then it drops back
// Closer than this it tails the car, settling police_tail_distance behind:
// well behind the chase camera (player_z, about four segments), where it
// looms in the rear-view mirror (any closer and it would sink below it).
constexpr float chase_tail_gap = 11.f;
constexpr float police_tail_distance = 9.f;
constexpr float police_min_gap = 7.f; // and never closer, braking as hard as the car must
constexpr float chase_catch_seconds = 3.f; // ... and for this long, it pulls it over
constexpr float pulled_over_seconds = 4.f; // stopped at the side of the road

enum class ChaseOutcome { Going, Escaped, Caught };

struct Chase {
    float time = 0.f;    // seconds since it started
    float tailing = 0.f; // seconds it has been tailing the car without a break
    bool giving_up = false; // out of time: it slows down, siren off, and drops back
};

// The police car's speed: flat out as fast as the standard car (`top`), so
// it gains when the car slows, and faster when far behind; close, it eases off to settle
// police_tail_distance behind the car, never passing it; giving up, it
// slows right down.
float police_speed(const Chase& chase, float player_speed, float gap, float top);

// One step of the chase with the gap now: who wins, or neither yet. The
// car has escaped once the police car is far enough behind, whether it lost
// the car or gave up.
ChaseOutcome update_chase(Chase& chase, float gap, float dt);

// Does a chase start this step? `roll` uniform in [0, 1).
bool chase_starts(float roll, float speed_pct, float dt);

} // namespace racer
