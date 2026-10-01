// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include "bitmap.hpp"
#include "framebuffer.hpp"
#include "track.hpp"

#include <vector>

namespace racer {

// Parallax backdrop: banded sky, clouds, far mountains and near hills. The
// layers sit on the horizon (the projection's eye level) and scroll sideways
// at different rates when the road bends.
class Background {
public:
    Background();

    // curve: bend of the player's segment; segments: distance travelled in
    // segment lengths this step.
    void update(float curve, float segments, float dt);
    void reset();

    void render(Framebuffer& fb, const RoadTheme& theme) const;

private:
    struct Cloud {
        int bitmap;
        float x;
        int y;
    };

    float sky_offset_ = 0.f;
    float mountain_offset_ = 0.f;
    float hill_offset_ = 0.f;
    float drift_ = 0.f;

    std::vector<float> mountains_; // height profile, one entry per pixel column
    std::vector<float> hills_;
    std::vector<Bitmap> cloud_bitmaps_;
    std::vector<Cloud> clouds_;
};

} // namespace racer
