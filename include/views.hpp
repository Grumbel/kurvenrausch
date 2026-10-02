// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>

namespace racer {

// Where the camera sits. Only the picture changes: the car drives the same
// in every view.
enum class ViewMode : uint8_t {
    Chase,   // behind the car, the classic view
    Far,     // higher up and further back
    Bumper,  // low, at the front of the car, which is not drawn
    Cockpit, // the driver's eyes, over the dashboard and the wheel
};
constexpr int view_modes = 4;

struct ViewSetup {
    float height;   // of the camera over the road, world units
    float distance; // from the camera forward to the car (negative: ahead of it)
    bool car;       // the car is drawn
    bool cockpit;   // the dashboard and the wheel are drawn
};

// For the chase camera's height and depth (see Camera); a truck's cab sits
// higher. The chase view's car touches the bottom screen row, the far view's
// a little above it.
ViewSetup view_setup(ViewMode mode, float chase_height, float depth, bool truck);
const char* view_name(ViewMode mode);

// Screen row (from the top, of `screen_h`) where the car meets the road in
// a view that draws it: half the height plus the camera's angle down to it.
float contact_row(const ViewSetup& v, float depth, int screen_h);

} // namespace racer
