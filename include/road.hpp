// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include "framebuffer.hpp"
#include "sprites.hpp"
#include "track.hpp"

#include <vector>

namespace racer {

// Everything the road renderer needs to know about the viewer.
struct RoadView {
    float position = 0.f;   // camera z along the track
    float player_x = 0.f;   // lateral offset in road half-widths
    float player_y = 0.f;   // road height under the car
    float camera_height = 1000.f;
    float camera_depth = 0.84f;
    float player_z = 840.f;
    int draw_distance = 300;
    float fog_density = 5.f;
    // +1 looks along the track. -1 looks back from `position`, as seen in a
    // rear-view mirror: left stays left, so the image needs no flipping.
    int direction = 1;
    // Turns the view away from the road's heading (world units sideways per
    // segment) and moves the camera sideways (world units): after taking the
    // other road at a fork the camera still stands and looks where it did on
    // the old one for a moment, and moves over smoothly.
    float yaw = 0.f;
    float shift = 0.f;
    // Screen row of eye level and vertical pixels per world unit at scale 1;
    // 0 means half the framebuffer height for either, as for the main view.
    float horizon = 0.f;
    float y_scale = 0.f;
};

struct ScreenPoint {
    float cam_z = 0.f;  // depth relative to the camera
    float scale = 0.f;  // camera_depth / cam_z
    float x = 0.f;      // screen position of the road centre
    float y = 0.f;
    float w = 0.f;      // screen half-width of the road
};

// A moving object on the road (traffic, the player's car), drawn in depth
// order together with the scenery.
struct RoadSprite {
    float z = 0.f;                 // position along the track
    const Bitmap* bitmap = nullptr;
    // Placement on the road, projected: centred on offset (road half-widths).
    float offset = 0.f;
    float world_width = 0.f;
    // Alternatively a fixed screen rectangle, not clipped by the terrain
    // (the player's car, which the camera always looks over).
    bool fixed = false;
    float sx = 0.f, sy = 0.f, sw = 0.f, sh = 0.f;
    float angle = 0.f; // fixed sprites only: rotation about the centre, clockwise
};

// Classic segment based pseudo-3D road renderer.
class RoadRenderer {
public:
    // `objects` may be in any order; it is sorted in place. Looking back, the
    // back of signs is drawn (`SpriteSheet::scenery_back`).
    void render(Framebuffer& fb, const Track& track, const RoadView& view,
                const SpriteSheet& sprites, std::vector<RoadSprite>& objects);

private:
    struct Slice {
        int index;          // segment index
        ScreenPoint p1, p2; // near and far edge (the segment's start and end
                            // looking forward, its end and start looking back)
        float clip;         // occlusion line from nearer road: draw only above
        float fog;          // 1 = clear, 0 = fully fogged
        bool road_visible;
    };

    void draw_segment(Framebuffer& fb, const Track& track, const Slice& s) const;
    // Guard rail or cliff along one side (-1 left, +1 right) of a segment.
    void draw_edge(Framebuffer& fb, const Track& track, const Slice& s, int side) const;
    void draw_sprites(Framebuffer& fb, const Track& track, const SpriteSheet& sprites,
                      const std::vector<RoadSprite>& objects) const;

    // Screen points of a slice at the segment's start and end along the track.
    const ScreenPoint& start(const Slice& s) const { return direction_ > 0 ? s.p1 : s.p2; }
    const ScreenPoint& end(const Slice& s) const { return direction_ > 0 ? s.p2 : s.p1; }

    std::vector<Slice> slices_;
    float camera_depth_ = 1.f;
    int direction_ = 1;
};

} // namespace racer
