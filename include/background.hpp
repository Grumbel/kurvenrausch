// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include "bitmap.hpp"
#include "framebuffer.hpp"
#include "track.hpp"

#include <vector>

namespace racer {

// How the backdrop is laid out in a view other than the main screen.
struct BackdropView {
    float horizon = 120.f; // screen row of eye level
    float zoom = 1.f;      // size relative to the main view
    // Looking back in a mirror: the layers show the half of the panorama
    // behind the car, mirrored, and there is no sun.
    bool mirror = false;
};

// Parallax backdrop: banded sky, clouds, far mountains and near hills. The
// layers sit on the horizon (the projection's eye level) and scroll sideways
// at different rates when the road bends. The sun and moon follow the hour
// of day across the sky (see sun_position / moon_position).
class Background {
public:
    Background();

    // curve: bend of the player's segment; segments: distance travelled in
    // segment lengths this step.
    void update(float curve, float segments, float dt);
    void reset();

    // The main view: horizon at half the screen height, zoom 1. `hour` is the
    // time of day (0 .. 24) for the sun and moon.
    void render(Framebuffer& fb, const RoadTheme& theme, float hour = 12.f) const;
    void render(Framebuffer& fb, const RoadTheme& theme, const BackdropView& view,
                float hour = 12.f) const;

    // Parallax state for the GLES backdrop path.
    static constexpr float sky_layer_period = 1280.f;
    float sky_offset() const { return sky_offset_ + drift_; } // clouds: bend + wind drift
    // Road-bend scroll only (no cloud wind). Stars / celestial sphere use this
    // so they turn with the world when the road curves.
    float sky_bend_offset() const { return sky_offset_; }
    float mountain_offset() const { return mountain_offset_; }
    float hill_offset() const { return hill_offset_; }
    const std::vector<float>& mountains() const { return mountains_; }
    const std::vector<float>& hills() const { return hills_; }

    struct CloudSprite {
        const Bitmap* bitmap = nullptr;
        float x = 0.f;         // position in the sky period
        float altitude = 0.f;  // top edge above the horizon (main-view pixels)
    };
    std::vector<CloudSprite> cloud_sprites() const;

private:
    struct Cloud {
        int bitmap;
        float x;
        float altitude; // of its top edge above the horizon, in main view pixels
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
