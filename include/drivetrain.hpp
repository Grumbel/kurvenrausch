// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <algorithm>
#include <cmath>

namespace racer::drivetrain {

// The car has an automatic gearbox with five virtual gears spread evenly over
// the speed range. The rev counter and the engine sound share this model.
constexpr int gears = 5;

// Current gear, 1 .. gears, for a speed given as a fraction of top speed.
inline int gear(float speed_fraction) {
    const float pct = std::clamp(speed_fraction, 0.f, 1.f);
    return std::min(gears, static_cast<int>(pct * static_cast<float>(gears)) + 1);
}

// Normalised revs, 0 .. 1: zero when standing, 0.25 just after a shift, 1 at
// the limit. The needle climbs through each gear and drops on the shift.
inline float rpm(float speed_fraction) {
    const float pct = std::clamp(speed_fraction, 0.f, 1.f);
    if (pct <= 0.f) return 0.f;
    const float g = pct * static_cast<float>(gears);
    const float in_gear = pct >= 1.f ? 1.f : g - std::floor(g);
    return 0.25f + 0.75f * in_gear;
}

} // namespace racer::drivetrain
