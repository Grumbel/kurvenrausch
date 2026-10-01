#pragma once
#include "types.hpp"
#include <string>
#include <vector>

namespace racer {

// --- Core game components (data-driven) ---

struct Transform {
    float x = 0.f;      // world lateral offset (-1..1 relative to road center)
    float z = 0.f;      // distance along track
    float y = 0.f;      // height (for camera / hills)
};

struct Velocity {
    float speed = 0.f;  // units per second along track
    float lateral = 0.f;
};

struct Player {
    float max_speed = 280.f;
    float accel = 120.f;
    float brake = 200.f;
    float steer = 2.5f;
    float centrifugal = 0.3f; // push outward on curves
};

struct Camera {
    float height = 1000.f;
    float depth = 0.84f;      // FOV related (camera to projection plane)
    float draw_distance = 300.f;
    float fog_density = 0.002f;
};

// Road segment data (the track is a circular buffer of these)
struct Segment {
    float curve = 0.f;        // dx per unit length (positive = right)
    float y = 0.f;            // absolute world height at start of segment
    Color color_road;
    Color color_grass;
    Color color_rumble;
    Color color_lane;
    int index = 0;
    // sprites attached to this segment (offset, scale, type)
    struct Sprite {
        float offset = 0.f;   // -1 left .. +1 right relative to road
        float scale = 1.f;
        int type = 0;         // 0=tree, 1=cliff, 2=billboard, etc.
        bool side = false;    // left or right preference
    };
    std::vector<Sprite> sprites;
};

// Track is pure data
struct Track {
    std::vector<Segment> segments;
    float segment_length = 200.f;
    float road_width = 2000.f;
    float rumble_width = 0.05f; // fraction of road
    int lanes = 3;
    float total_length = 0.f;

    void rebuild_lengths() {
        total_length = segments.size() * segment_length;
    }

    Segment& get(int idx) {
        int n = static_cast<int>(segments.size());
        idx = ((idx % n) + n) % n;
        return segments[idx];
    }

    const Segment& get(int idx) const {
        int n = static_cast<int>(segments.size());
        idx = ((idx % n) + n) % n;
        return segments[idx];
    }

    int find_segment_index(float z) const {
        int idx = static_cast<int>(z / segment_length);
        int n = static_cast<int>(segments.size());
        return ((idx % n) + n) % n;
    }
};

// Render-projected point for a segment (computed each frame)
struct Projected {
    float x = 0.f, y = 0.f, w = 0.f; // screen coords + half-width
    float scale = 0.f;
    float clip = 0.f; // for hill clipping
};

} // namespace racer
