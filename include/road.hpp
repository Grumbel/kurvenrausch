#pragma once
#include "components.hpp"
#include "renderer.hpp"
#include <vector>

namespace racer {

// Data-driven track builder and road projection system.
// Implements classic pseudo-3D: segments with curve + height,
// projected with 1/z, cumulative dx for bends, dy for hills.

class RoadSystem {
public:
    void build_demo_track(Track& track);
    void project_segments(const Track& track, float player_z, float player_x,
                          const Camera& cam, int screen_w, int screen_h,
                          std::vector<Projected>& projected,
                          float& max_y);
    void render(Renderer& r, const Track& track,
                const std::vector<Projected>& projected,
                float player_z, float player_x, const Camera& cam);

private:
    void add_segment(Track& t, float curve, float y);
    void add_road(Track& t, int enter, int hold, int leave, float curve, float y);
    void add_sprite(Track& t, int seg, float offset, int type, float scale = 1.f);
};

} // namespace racer
