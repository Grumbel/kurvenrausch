// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "police.hpp"

#include <algorithm>
#include <cmath>

namespace racer {

PoliceMove police_move(const Chase& chase, float police_speed, float player_speed, float player_x,
                       float gap, float top, float dt) {
    // The side to pass on: the one with more road beside the car.
    const float beside = std::clamp(player_x + (player_x > 0.f ? -chase_lane_offset : chase_lane_offset), -0.8f, 0.8f);
    if (chase.giving_up || chase.phase == ChasePhase::Leaving) {
        return {chase.giving_up ? 0.5f * top : 0.9f * top, beside};
    }
    switch (chase.phase) {
        case ChasePhase::Closing:
            // Flat out, harder when far behind so the chase isn't over
            // before it began.
            return {top * (1.f + 0.08f * std::clamp((gap - 20.f) / 25.f, 0.f, 1.f)), player_x};
        case ChasePhase::Overtaking:
            return {std::max(player_speed, 0.f) + 0.15f * top, beside};
        default: {
            // Blocking: into the car's lane and down to a stop, gently, so
            // the car can brake behind it; well ahead, slower still so the
            // car catches up.
            const float brake = (gap < -2.f * chase_block_gap ? 0.5f : 0.25f) * top * dt;
            return {std::max(0.f, std::min(police_speed, std::max(player_speed, 0.f) + 0.05f * top) - brake),
                    player_x};
        }
    }
}

ChaseOutcome update_chase(Chase& chase, float gap, float player_speed_pct, float lateral, float dt) {
    chase.time += dt;
    if (chase.time > chase_give_up && chase.phase != ChasePhase::Leaving) chase.giving_up = true;
    if (chase.phase == ChasePhase::Leaving) return ChaseOutcome::Going;
    if (gap > chase_escape_gap) return ChaseOutcome::Escaped;
    if (chase.giving_up) return ChaseOutcome::Going;
    switch (chase.phase) {
        case ChasePhase::Closing:
            if (gap < chase_overtake_gap) chase.phase = ChasePhase::Overtaking;
            break;
        case ChasePhase::Overtaking:
            if (gap < -chase_block_gap) chase.phase = ChasePhase::Blocking;
            break;
        case ChasePhase::Blocking:
            if (gap > 0.5f) { // the car got past it
                chase.phase = ChasePhase::Closing;
                chase.stopped = 0.f;
                break;
            }
            // Stopped right behind it, in its lane.
            if (gap > -2.f * chase_block_gap && std::abs(lateral) < 0.5f && player_speed_pct < chase_stop_speed) {
                chase.stopped += dt;
            } else {
                chase.stopped = 0.f;
            }
            if (chase.stopped >= chase_stop_seconds) {
                chase.phase = ChasePhase::Leaving;
                return ChaseOutcome::Caught;
            }
            break;
        default: break;
    }
    return ChaseOutcome::Going;
}

bool chase_starts(float roll, float speed_pct, float dt) {
    return speed_pct >= chase_min_speed && roll < chase_rate * dt;
}

} // namespace racer
