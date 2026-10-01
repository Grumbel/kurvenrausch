// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include "types.hpp"

#include <cstdint>
#include <string>
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
    Fir,
    SnowFir,
    Cactus,
    Mesa,         // large sandstone butte, a landmark far from the road
    Cypress,
    Chalet,
    RedRock,
    Pole,         // telephone pole
    DryShrub,
    BillboardUs,
    Count
};

struct SceneryInfo {
    float width;    // world units; the bitmap's aspect ratio gives the height
    bool solid;     // the player crashes into it
    bool centered;  // centred on its offset instead of aligned by its inner edge
    bool mirrorable;// may be drawn mirrored on the left side for variety
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
    // Backdrop
    Color cloud[3] = {{0xb8, 0xc8, 0xe0}, {0xe4, 0xec, 0xf6}, {0xff, 0xff, 0xff}};
    Color mountain_lit{0x8c, 0x9c, 0xc8};
    Color mountain_shade{0x6c, 0x78, 0xac};
    Color snow{0xec, 0xf2, 0xfa};
    Color hill_lit{0x5c, 0xa0, 0x5c};
    Color hill_shade{0x48, 0x88, 0x4c};
    // Atmosphere
    float fog_density = 5.f;  // exponential fog; larger is thicker
};

// Blends two looks: colours and numbers interpolate, anything discrete is
// taken from the nearer one. Used to fade smoothly between zones.
RoadTheme mix_themes(const RoadTheme& a, const RoadTheme& b, float t);

// A stretch of track with its own country, scenery and atmosphere.
struct Zone {
    std::string country;
    std::string region;
    RoadTheme theme;
    int first_segment = 0;  // set by the track builder
};

// A looping track: a circular array of fixed-length segments.
struct Track {
    std::vector<Segment> segments;
    float segment_length = 200.f;
    float road_width = 2000.f;  // half width in world units
    int lanes = 3;
    float start_z = 0.f;        // position of the start/finish line

    // Zones in track order; zones[0] starts at segment 0 and the last one runs
    // into the first across the lap seam. finish() derives the per-segment
    // data below and must be called once the segments and zones are set up.
    std::vector<Zone> zones;
    std::vector<int> zone_index;       // zone of each segment
    std::vector<RoadTheme> looks;      // blended look of each segment

    // Transitions between zones are spread over this many segments (clamped to
    // the shortest zone), centred on the boundary.
    void finish(int transition_segments = 100);

    const RoadTheme& look(int segment) const;
    const RoadTheme& look_at(float z) const { return look(index_at(z)); }
    const Zone& zone_at(float z) const;
    int zone_number_at(float z) const;

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
