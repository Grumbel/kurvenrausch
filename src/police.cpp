// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "police.hpp"

#include <algorithm>

namespace racer {

float police_speed(const Chase& chase, float player_speed, float gap, float top) {
    if (chase.giving_up) return 0.5f * top;
    // Flat out, harder when far behind so the chase isn't over before it
    // began; closing in, it eases off towards the car's speed, settling a
    // little behind it, and drops back should it ever get ahead.
    const float flat_out = top * (1.f + 0.08f * std::clamp((gap - 20.f) / 25.f, 0.f, 1.f));
    const float settle = player_speed + (gap - police_tail_distance) * 0.25f * top;
    return std::clamp(settle, 0.f, flat_out);
}

ChaseOutcome update_chase(Chase& chase, float gap, float dt) {
    chase.time += dt;
    if (chase.time > chase_give_up) chase.giving_up = true;
    const bool tailing = !chase.giving_up && gap >= 0.f && gap < chase_tail_gap;
    chase.tailing = tailing ? chase.tailing + dt : 0.f;
    if (chase.tailing >= chase_catch_seconds) return ChaseOutcome::Caught;
    if (gap > chase_escape_gap) return ChaseOutcome::Escaped;
    return ChaseOutcome::Going;
}

bool chase_starts(float roll, float speed_pct, float dt) {
    return speed_pct >= chase_min_speed && roll < chase_rate * dt;
}

} // namespace racer
