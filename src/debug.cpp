// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "debug.hpp"

#include <cmath>

namespace racer {

void step_hour(float& hour, int step) {
    hour = std::fmod(hour + static_cast<float>(step), 24.f);
    if (hour < 0.f) hour += 24.f;
}

} // namespace racer
