#pragma once
#include "types.hpp"
#include <vector>

namespace racer {

struct Transform {
    float x = 0.f;   // lateral position in road units (-1..1 typical)
    float z = 0.f;   // distance along track
    float y = 0.f;   // world height
};

struct Velocity {
    float speed = 0.f;
};

struct Player {
    float max_speed   = 300.f;
    float accel       = 140.f;
    float brake       = 220.f;
    float steer_speed = 3.0f;
    float centrifugal = 0.35f;
};

struct Camera {
    float height        = 1000.f;   // camera height above road
    float depth         = 0.84f;    // FOV-ish (camera distance to projection plane)
    float draw_distance = 200.f;    // how many segments ahead to draw
    float fog_density   = 5.f;
};

struct Segment {
    float curve = 0.f;   // curvature (dx added per segment)
    float y     = 0.f;   // world Y at this segment
    Color color_road;
    Color color_grass;
    Color color_rumble;
    Color color_lane;
    int   index = 0;

    struct Sprite {
        float offset = 0.f;  // -1 left edge .. +1 right edge of road
        float scale  = 1.f;
        int   type   = 0;    // 0 tree, 1 cliff, 2 billboard
    };
    std::vector<Sprite> sprites;
};

struct Track {
    std::vector<Segment> segments;
    float segment_length = 200.f;
    float road_width     = 2000.f;
    float rumble_width   = 0.1f;   // fraction of half-road
    int   lanes          = 3;
    float total_length   = 0.f;

    void rebuild() {
        total_length = static_cast<float>(segments.size()) * segment_length;
        for (size_t i = 0; i < segments.size(); ++i)
            segments[i].index = static_cast<int>(i);
    }

    Segment& get(int idx) {
        int n = static_cast<int>(segments.size());
        if (n == 0) { static Segment dummy; return dummy; }
        idx = ((idx % n) + n) % n;
        return segments[idx];
    }
    const Segment& get(int idx) const {
        int n = static_cast<int>(segments.size());
        if (n == 0) { static Segment dummy; return dummy; }
        idx = ((idx % n) + n) % n;
        return segments[idx];
    }

    int index_from_z(float z) const {
        int n = static_cast<int>(segments.size());
        if (n == 0) return 0;
        int idx = static_cast<int>(std::floor(z / segment_length));
        return ((idx % n) + n) % n;
    }
};

// Screen-space projection of one segment point
struct Projected {
    float x     = 0.f;  // screen center x of road
    float y     = 0.f;  // screen y
    float w     = 0.f;  // half-width of road on screen
    float scale = 0.f;
    float clip  = 0.f;  // y-clip for hills
};

} // namespace racer
