// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "views.hpp"

namespace racer {

ViewSetup view_setup(ViewMode mode, float chase_height, float depth, bool truck) {
    switch (mode) {
        case ViewMode::Far: return {1.3f * chase_height, 1.6f * chase_height * depth, true, false};
        case ViewMode::Bumper: return {0.3f * chase_height, -300.f, false, false};
        case ViewMode::Cockpit: return {(truck ? 0.9f : 0.5f) * chase_height, 100.f, false, true};
        default: return {chase_height, chase_height * depth, true, false};
    }
}

const char* view_name(ViewMode mode) {
    switch (mode) {
        case ViewMode::Far: return "FAR VIEW";
        case ViewMode::Bumper: return "BUMPER VIEW";
        case ViewMode::Cockpit: return "COCKPIT VIEW";
        default: return "CHASE VIEW";
    }
}

float contact_row(const ViewSetup& v, float depth, int screen_h) {
    const float half = static_cast<float>(screen_h) / 2.f;
    return half + half * v.height * depth / v.distance;
}

} // namespace racer
