#pragma once
#include "types.hpp"

#include <cstdint>
#include <vector>

namespace racer {

// Kinds of roadside objects. Their physical size lives in scenery_info(),
// their look in the sprite sheet.
enum class Scenery : uint8_t {
    Palm,
    Tree,
    Bush,
    Boulder,
    Billboard,
    Gantry,   // start/finish banner spanning the road
};

struct SceneryInfo {
    float width;    // world units; the bitmap's aspect ratio gives the height
    bool solid;     // the player crashes into it
    bool centered;  // centred on its offset instead of aligned by its inner edge
};

const SceneryInfo& scenery_info(Scenery kind);

struct RoadsideObject {
    Scenery kind;
    // Lateral position in road half-widths: 0 is the centre line, +-1 the road
    // edges. Unless centred, the object's inner edge sits at this offset, so it
    // extends away from the road.
    float offset;
};

struct Segment {
    float curve = 0.f;     // lateral bend per segment (positive bends right)
    float y1 = 0.f;        // world height at the near edge
    float y2 = 0.f;        // world height at the far edge
    bool alt = false;      // alternating colour band (rumble / grass stripes)
    bool checker = false;  // start/finish line
    std::vector<RoadsideObject> scenery;
};

// Colours of a track. Pure data so tracks can be themed.
struct RoadTheme {
    Color sky_top{0x30, 0x60, 0xd0};
    Color sky_horizon{0xb8, 0xdc, 0xf4};
    Color fog{0xc8, 0xe0, 0xe8};
    Color grass[2] = {{0x48, 0xa0, 0x38}, {0x3c, 0x90, 0x2e}};
    Color road[2] = {{0x74, 0x74, 0x74}, {0x6c, 0x6c, 0x6c}};
    Color rumble[2] = {{0xf0, 0xf0, 0xf0}, {0xd0, 0x20, 0x20}};
    Color lane{0xe8, 0xe8, 0xe8};
    Color checker[2] = {{0xf8, 0xf8, 0xf8}, {0x18, 0x18, 0x18}};
};

// A looping track: a circular array of fixed-length segments.
struct Track {
    std::vector<Segment> segments;
    float segment_length = 200.f;
    float road_width = 2000.f;  // half width in world units
    int lanes = 3;
    float start_z = 0.f;        // position of the start/finish line
    RoadTheme theme;

    float length() const { return static_cast<float>(segments.size()) * segment_length; }

    // Wraps z into [0, length()).
    float wrap(float z) const;

    int index_at(float z) const;
    const Segment& segment(int index) const;
    const Segment& segment_at(float z) const { return segment(index_at(z)); }

    // Road surface height at z, interpolated within the segment.
    float height_at(float z) const;
};

Track build_demo_track();

} // namespace racer
