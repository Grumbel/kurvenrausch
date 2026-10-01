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
    GasStation,   // shop under a canopy, beyond the forecourt
    FuelPump,
    FuelSign,     // tall sign announcing a gas station
    Count
};

struct SceneryInfo {
    float width;    // world units; the bitmap's aspect ratio gives the height
    bool solid;     // the player crashes into it
    bool centered;  // centred on its offset instead of aligned by its inner edge
    bool mirrorable;// may be drawn mirrored on the left side for variety
};

const SceneryInfo& scenery_info(Scenery kind);

// A continuous roadside feature along one side of a segment.
enum class Edge : uint8_t {
    None,
    Rail,   // guard rail; beyond it the ground drops away (sea, valley)
    Cliff,  // rock wall rising beside the road
};

// Lateral position of the edge features, in road half-widths, and their size.
constexpr float rail_offset = 1.22f;
constexpr float cliff_offset = 1.40f;
constexpr float rail_height = 330.f;    // world units
constexpr float cliff_height = 3600.f;  // typical; varies along the road

// A gas station's forecourt: paved ground on the right of the road, out to
// this offset (road half-widths), where the car can pull in and refuel.
constexpr float forecourt_width = 2.3f;

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
    Edge left = Edge::None;
    Edge right = Edge::None;
    float edge_fade = 1.f; // 0..1, cliffs grow and shrink at the ends of a run
    // Outer edge of a gas station forecourt on the right, in road half-widths
    // (up to forecourt_width), or 0 for none. It widens and narrows at the
    // ends; refuelling works where it is full width.
    float forecourt = 0.f;
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

    // Roadside edges
    Color rock[3] = {{0x6a, 0x50, 0x3c}, {0x9a, 0x7a, 0x58}, {0xc4, 0xa0, 0x78}}; // dark, mid, light
    Color cap{0xf0, 0xf4, 0xfa};      // top layer of cliffs (snow, grass)
    float cap_amount = 0.f;           // 0 = bare rock .. 1 = thick cap
    Color rail[2] = {{0xd8, 0xdc, 0xe0}, {0x70, 0x74, 0x7c}}; // bars, posts
    Color beyond[2] = {{0x28, 0x78, 0xc0}, {0x30, 0x84, 0xcc}}; // ground past a rail: sea

    // Sky and backdrop
    Color cloud_tint{0x60, 0x68, 0x78}; // clouds are shaded towards this ...
    float cloud_tint_amount = 0.f;      // ... by this much (storm, sunset)
    Color sun{0xff, 0xf2, 0xc0};
    float sun_amount = 0.f;             // 0 = no visible sun
    float haze = 0.f;                   // extra haze on mountains and hills, 0 .. 1
    float mountain_scale = 1.f;         // height of the far mountains
    float hill_scale = 1.f;             // height of the near hills
    float snow_line = 40.f;             // mountains are white above this height (pixels)

    // Weather
    float rain = 0.f;                   // 0 .. 1
    float snowfall = 0.f;               // 0 .. 1
    float grip = 1.f;                   // tyre grip: 1 dry road, less when wet or icy

    // Road markings (discrete: the nearer zone wins while blending)
    int lanes = 3;                      // 2 or 3
    bool us_markings = false;           // yellow double centre line, white edge lines
    Color center_line{0xe8, 0xc0, 0x20};
};

// Lateral centre of lane `index` (0 = leftmost) on a road with `lanes` lanes,
// in road half-widths: -2/3, 0, 2/3 for three lanes, -1/2, 1/2 for two.
inline float lane_center(int lanes, int index) {
    return (2.f * static_cast<float>(index) + 1.f) / static_cast<float>(lanes) - 1.f;
}

// Blends two looks: colours and numbers interpolate, anything discrete is
// taken from the nearer one. Used to fade smoothly between zones.
RoadTheme mix_themes(const RoadTheme& a, const RoadTheme& b, float t);

// Rules for what is planted along the road in a zone.
enum class Decor : uint8_t {
    Riviera,   // palm avenues, billboards
    Forest,    // dense firs
    Alpine,    // snowy firs, chalets, boulders
    Tuscany,   // cypress avenues, oaks, bushes
    Desert,    // cacti, shrubs, red rocks, mesas, telephone poles
    Coast,     // sparse: shrubs, rocks, poles where the sides are free
};

// A stretch of track with its own country, scenery and atmosphere.
struct Zone {
    std::string country;
    std::string region;
    RoadTheme theme;
    int first_segment = 0;  // set by the track builder
    Decor decor = Decor::Riviera;
};

// A looping track: a circular array of fixed-length segments.
struct Track {
    std::vector<Segment> segments;
    float segment_length = 200.f;
    float road_width = 2000.f;  // half width in world units
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

    // World height of the edge feature on `side` (-1 left, +1 right) at the
    // boundary in front of segment `boundary`, i.e. between segments boundary-1
    // and boundary. Cliffs vary along the road and taper to nothing at the ends
    // of a run, so the wall never starts or stops abruptly.
    float edge_height(int boundary, int side) const;

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

    // Outer edge of the forecourt at the boundary in front of segment
    // `boundary`: the narrower of the segments either side, so it starts and
    // ends with a taper.
    float forecourt_at(int boundary) const;

    // Is lateral position x (road half-widths) on the paved forecourt of the
    // segment at z?
    bool on_forecourt(float z, float x) const;

    // First segments of each forecourt at full width, in track order.
    std::vector<int> gas_stations() const;
};

// How far from the centre line (in road half-widths) a car of the given half
// width may get on `side` (-1 left, +1 right) before it touches a rail or
// cliff on this segment; infinity if there is nothing there. A cliff that has
// not grown to a worthwhile height yet does not count.
float barrier_limit(const Segment& seg, int side, float car_half_width);

// Did moving from `prev_z` to `z` (positions along a looping track of the given
// length, either may have wrapped) carry the car forward over the line at
// `line_z`? Stepping backwards, as after a collision, never counts, and nor
// does crossing the line backwards.
bool crossed_line_forward(float prev_z, float z, float line_z, float length);

Track build_demo_track();

// Plan view of a track for the mini map, one point per segment start, inside
// the unit square (centred, aspect kept, y down). A pseudo-3D track is not a
// geometric loop: its bends are only lateral offsets, and integrating them as
// turns gives neither one full turn nor a closed path. So the bends are
// damped by `bend_scale` (sharp ones would otherwise show as hairpins), the
// turning missing to a full turn is spread evenly over the lap, and what is
// left of the gap at the end is spread out along the path.
struct MapPoint {
    float x, y;
};
std::vector<MapPoint> track_map(const Track& track, float bend_scale = 0.6f);

} // namespace racer
