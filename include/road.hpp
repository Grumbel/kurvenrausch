#pragma once
#include "framebuffer.hpp"
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
};

struct ScreenPoint {
    float cam_z = 0.f;  // depth relative to the camera
    float scale = 0.f;  // camera_depth / cam_z
    float x = 0.f;      // screen position of the road centre
    float y = 0.f;
    float w = 0.f;      // screen half-width of the road
};

// Classic segment based pseudo-3D road renderer.
class RoadRenderer {
public:
    void render(Framebuffer& fb, const Track& track, const RoadView& view);

private:
    struct Slice {
        int index;          // segment index
        ScreenPoint p1, p2; // near and far edge
        float clip;         // occlusion line from nearer road: draw only above
        float fog;          // 1 = clear, 0 = fully fogged
        bool road_visible;
    };

    void draw_segment(Framebuffer& fb, const Track& track, const Slice& s) const;
    void draw_scenery(Framebuffer& fb, const Track& track) const;

    std::vector<Slice> slices_;
    float camera_depth_ = 1.f;
};

} // namespace racer
