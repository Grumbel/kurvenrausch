// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include "daylight.hpp"
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
    // Distance fog / air colour from the current look (weather, FOGGY, …).
    // Must match Background; do not re-derive from track.look per segment.
    Color fog_air{};
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
    // Horizontal pixels per world unit at scale 1; 0 means half the
    // framebuffer width. A framebuffer wider than 4:3 keeps the 4:3 scale and
    // shows more to the sides.
    float x_scale = 0.f;
    // World units ahead of the camera: windowed buildings inside this range
    // draw with every window lit (horn at night in a city). 0 = off.
    float window_wake = 0.f;
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
    bool flip = false; // drawn mirrored (an animal walking the other way)
    // Lit at night: headlights shining away from the camera (+1, the car's
    // back seen) or towards it (-1, its front seen), and tail lights.
    int lights = 0;
    // Alternatively a fixed screen rectangle, not clipped by the terrain
    // (the player's car, which the camera always looks over).
    bool fixed = false;
    float sx = 0.f, sy = 0.f, sw = 0.f, sh = 0.f;
    float angle = 0.f; // fixed sprites only: rotation about the centre, clockwise
    // GLES: re-upload this bitmap each frame (animated overlay whose pixels change).
    bool dynamic = false;
};

// Classic segment based pseudo-3D road renderer.
class RoadRenderer {
public:
    // `objects` may be in any order; it is sorted in place. Looking back, the
    // back of signs is drawn (`SpriteSheet::scenery_back`).
    void render(Framebuffer& fb, const Track& track, const RoadView& view,
                const SpriteSheet& sprites, std::vector<RoadSprite>& objects);
    // After render(): how far ahead of the camera (world units) the road or
    // ground seen in each screen row lies; 0 where none is seen (the sky).
    const std::vector<float>& row_depth() const { return row_depth_; }
    // After render(): the street lamps and the lit vehicles drawn, where
    // their light falls.
    const std::vector<LampSpot>& lamps() const { return lamps_; }
    // After render(): the picture with the road and ground drawn but nothing
    // standing on them, to tell the ground from what stands on it.
    const std::vector<uint32_t>& ground() const { return ground_; }

private:
    struct Slice {
        int index;          // segment index
        ScreenPoint p1, p2; // near and far edge (the segment's start and end
                            // looking forward, its end and start looking back)
        float clip;         // occlusion line from nearer road: draw only above
        float top;          // occlusion line from nearer tunnel ceilings: draw only below
        float left, right;  // seen through a tunnel's mouth: draw only between
        float fog;          // 1 = clear, 0 = fully fogged
        bool road_visible;
    };

    void draw_segment(Framebuffer& fb, const Track& track, const Slice& s) const;
    // Guard rail or cliff along one side (-1 left, +1 right) of a segment.
    void draw_edge(Framebuffer& fb, const Track& track, const Slice& s, int side,
                   const Bitmap& cliff) const;
    void draw_sprites(Framebuffer& fb, const Track& track, const SpriteSheet& sprites,
                      const std::vector<RoadSprite>& objects) const;

    // Screen points of a slice at the segment's start and end along the track.
    const ScreenPoint& start(const Slice& s) const { return direction_ > 0 ? s.p1 : s.p2; }
    const ScreenPoint& end(const Slice& s) const { return direction_ > 0 ? s.p2 : s.p1; }

    std::vector<Slice> slices_;
    std::vector<float> row_depth_;
    mutable std::vector<LampSpot> lamps_; // found while drawing the scenery
    std::vector<uint32_t> ground_;
    float camera_depth_ = 1.f;
    float x_scale_ = 1.f;
    float y_scale_ = 1.f;
    int direction_ = 1;
    Color fog_air_{};
    float window_wake_ = 0.f;
};

} // namespace racer
