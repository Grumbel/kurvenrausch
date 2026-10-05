// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once
#include "types.hpp"

#include <cmath>
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
    Victorian,    // San Francisco row houses, in three colours
    VictorianB,
    VictorianC,
    StreetLamp,
    SignLeft,     // a fork ahead: this way to the left route
    SignRight,    // ... and to the right one
    Hedge,        // English hedgerow
    StoneWall,    // dry-stone wall
    PhoneBox,     // red telephone box
    Windmill,
    Tulips,       // a strip of tulip field
    CherryTree,   // in blossom
    Torii,        // shrine gate
    StoneLantern,
    Showroom,     // a car dealer's showroom, beyond its forecourt
    DealerSign,   // the tall sign announcing it
    DatePalm,
    Pyramid,      // a landmark far from the road
    Acacia,       // flat-topped savanna tree
    Giraffe,
    TermiteMound,
    Banyan,
    Temple,       // a Hindu temple with its tower
    Cow,
    Maple,        // in autumn red
    HanokGate,    // Korean gate with a curved tiled roof
    GumTree,      // eucalyptus
    KangarooSign,
    Uluru,        // the great red rock, a landmark far from the road
    JungleTree,
    Banana,
    CarWash,      // the wash bay with its brushes, beyond its forecourt
    WashSign,     // the tall sign announcing it
    Motel,        // a row of rooms, beyond its forecourt
    MotelSign,    // the neon sign announcing it
    Hospital,     // beyond its forecourt
    HospitalSign, // the blue H sign announcing it
    Truckstop,    // a diner with a big sign, beyond its forecourt
    TruckSign,    // the tall sign announcing it
    SportsShowroom, // a sports car dealer's showroom, beyond its forecourt
    SportsSign,   // the tall sign announcing it
    CrossingSign, // a railway crossbuck with its two red lamps
    TunnelPortal, // the rock face round a tunnel's mouth, centred over the road
    BridgeTruss,  // a steel truss frame spanning a bridge
    Overpass,     // a road bridge crossing over the road
    ChemicalPlant,// tanks, pipes and a flare stack, beyond its forecourt
    ChemicalSign, // the sign announcing it: NOS
    Casino,       // a casino hotel tower, its sign in lights
    CasinoPyramid,// a black glass pyramid, a beam of light from its tip
    NeonSign,     // a tall neon pylon on the Strip
    Townhouse,    // a European town house: plaster, shutters, a tiled roof
    TownhouseB,   // ... in other colours
    Shop,         // a shop with its awning, flats above
    Apartment,    // a tall block of flats with balconies
    Tower,        // a glass office tower
    FlatHouse,    // a flat-roofed house of sandstone or plaster, warm climates
    GoldenGate,   // San Francisco: Art Deco tower portal you drive through (the road is the bridge)
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

// Patches on the road surface.
enum class Patch : uint8_t {
    None,
    Water, // a puddle: aquaplaning at speed, spray
    Oil,   // an oil slick: hardly any grip at all
};

// What a forecourt belongs to.
enum class Lot : uint8_t {
    Gas,    // a gas station: refuel
    Dealer, // a car dealer: change cars (the everyday ones)
    Wash,   // a car wash: clean the car
    Motel,  // a motel: change the passenger
    Hospital, // a hospital: patch up the driver, or change drivers
    Truckstop, // a truck stop: trucks and vans
    SportsDealer, // a sports car dealer
    Chemical, // a chemical plant: the nitro canisters filled again
};
constexpr int lot_kinds = 8;
// The name the HUD shows, and the keyword --visit takes.
const char* lot_name(Lot kind);
const char* lot_keyword(Lot kind);

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
    bool rails = false;    // a level crossing: a railway crosses the road here
    bool tunnel = false;   // inside a tunnel: walls beside the road, a ceiling over it
    Edge left = Edge::None;
    Edge right = Edge::None;
    float edge_fade = 1.f; // 0..1, cliffs grow and shrink at the ends of a run
    // Outer edge of a gas station forecourt on the right, in road half-widths
    // (up to forecourt_width), or 0 for none. It widens and narrows at the
    // ends; refuelling works where it is full width.
    float forecourt = 0.f;
    int8_t court_side = 1;  // the forecourt's side: +1 right, -1 left (where traffic keeps left)
    Lot lot = Lot::Gas;    // whose forecourt it is
    // A patch on the road surface (a puddle or an oil slick): its kind, centre
    // and half width in road half-widths; a half width of 0 means none. It
    // swells and shrinks along a few segments.
    Patch patch = Patch::None;
    float patch_x = 0.f;
    float patch_w = 0.f;
    // On a route of a fork, the side facing the other route (-1 left, +1
    // right): scenery there stays between the two roads. Where the routes
    // bend apart or together (branch_bend), it stays clear altogether.
    int8_t facing_branch = 0;
    bool branch_bend = false;
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
    float stars = 0.f;                  // 0 .. 1, stars in the night sky (see at_daytime())
    float haze = 0.f;                   // extra haze on mountains and hills, 0 .. 1
    float night_glow = 0.15f;           // light at night: 0 the dark outback .. 1 a big city
    float mountain_scale = 1.f;         // height of the far mountains
    float hill_scale = 1.f;             // height of the near hills
    float snow_line = 40.f;             // mountains are white above this height (pixels)

    // Weather
    float rain = 0.f;                   // 0 .. 1
    float snowfall = 0.f;               // 0 .. 1
    float showers = 0.3f;               // the rain a passing storm brings on top, 0 .. 1 (see weathered())
    float grip = 1.f;                   // tyre grip: 1 dry road, less when wet or icy
    float road_scale = 1.f;             // road width, of the standard Track::road_width

    // Road markings (discrete: the nearer zone wins while blending)
    int lanes = 3;                      // 2 or 3
    bool us_markings = false;           // yellow double centre line, white edge lines
    bool left_hand = false;             // traffic keeps to the left (England, Japan, ...)
    Color center_line{0xe8, 0xc0, 0x20};
};

// Lateral centre of lane `index` (0 = leftmost) on a road with `lanes` lanes,
// in road half-widths: -2/3, 0, 2/3 for three lanes, -1/2, 1/2 for two.
inline float lane_center(int lanes, int index) {
    return (2.f * static_cast<float>(index) + 1.f) / static_cast<float>(lanes) - 1.f;
}

// A tunnel's inside: the ceiling this high over the road, the walls this
// far out (road half-widths of the standard road), as the portal's opening.
constexpr float tunnel_height = 2600.f;
constexpr float tunnel_half_width = 1.25f;

// Every road carries traffic both ways: one lane comes towards the player,
// the leftmost where traffic keeps to the right, the rightmost where it
// keeps to the left; the others (one on a two-lane road, two on a
// three-lane one) go the player's way.
inline int oncoming_lane(int lanes, bool left_hand) { return left_hand ? lanes - 1 : 0; }
// The middle of the lanes going the player's way, in road half-widths.
inline float own_side(int lanes, bool left_hand) {
    return -lane_center(lanes, oncoming_lane(lanes, left_hand)) / static_cast<float>(lanes - 1);
}
// The lane going the player's way nearest to x.
inline int nearest_own_lane(int lanes, bool left_hand, float x) {
    int best = -1;
    for (int i = 0; i < lanes; ++i) {
        if (i == oncoming_lane(lanes, left_hand)) continue;
        if (best < 0 || std::abs(lane_center(lanes, i) - x) < std::abs(lane_center(lanes, best) - x)) best = i;
    }
    return best;
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
    City,      // rows of Victorian houses along the sidewalks, street lamps
    Town,      // the cities of the long tour: town houses, shops and blocks of flats, the country's trees
    Country,   // English lanes: hedgerows, stone walls, oaks, the odd phone box
    Polder,    // Dutch flatland: windmills, tulip fields
    Sakura,    // Japan: cherry trees, shrine gates, stone lanterns
    Nile,      // Egypt: date palms along the river, pyramids beyond
    Savanna,   // Kenya: acacias, giraffes, termite mounds
    Rajasthan, // India: banyans, temples, cows by the road
    Autumn,    // Korea: red maples, hanok gates
    Outback,   // Australia: gum trees, kangaroo signs, termite mounds, Uluru
    Jungle,    // Brazil: dense rainforest, banana plants
};

// What the buildings of a city (Decor::Town) are like.
enum class TownStyle : uint8_t {
    European, // town houses with tiled roofs, shops, some blocks of flats
    Modern,   // office towers and blocks of flats
    Warm,     // flat-roofed houses, shops, blocks of flats
    Strip,    // casinos in lights, neon signs (Las Vegas)
};

// A stretch of track with its own country, scenery and atmosphere.
struct Zone {
    std::string country;
    std::string region;
    RoadTheme theme;
    int first_segment = 0;  // set by the track builder
    Decor decor = Decor::Riviera;
    Scenery town_tree = Scenery::Tree; // Decor::Town: the trees between the houses ...
    TownStyle town = TownStyle::European; // ... and the houses
};

// A fork: the road splits into two routes of the same length that join again
// further on. Only the chosen route's segments are in Track::segments;
// choose_branch() swaps them. As both are equally long, every position after
// the join is the same whichever way the car went.
struct Branch {
    int fork = 0;                   // first segment of the routes
    int length = 0;                 // segments in each route
    int bend = 0;                   // segments at each end where the routes part and meet
    std::string names[2];           // the left and the right route
    std::vector<Segment> routes[2]; // both routes; the active one is also in Track::segments
    int active = 1;
    int end() const { return fork + length; }
};

// A looping track: a circular array of fixed-length segments.
struct Track {
    std::vector<Segment> segments;
    float segment_length = 200.f;
    float road_width = 2000.f;  // standard half width in world units; looks scale it

    // Half width of the road in world units at the boundary in front of
    // segment `boundary`, and anywhere along the track (interpolated). Lateral
    // positions in road half-widths convert with these.
    float half_width(int boundary) const;
    float half_width_at(float z) const;
    float start_z = 0.f;        // position of the start/finish line

    // Zones in track order; zones[0] starts at segment 0 and the last one runs
    // into the first across the lap seam. finish() derives the per-segment
    // data below and must be called once the segments and zones are set up.
    std::vector<Zone> zones;
    std::vector<int> zone_index;       // zone of each segment
    std::vector<RoadTheme> looks;      // blended look of each segment
    std::vector<float> branch_offsets; // per boundary, see branch_offset()
    std::vector<float> branch_slopes;  // per boundary, see branch_slope()

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

    // First segments of each forecourt of this kind at full width, in track
    // order.
    std::vector<int> lots(Lot kind) const;
    // The level crossings, by segment.
    std::vector<int> crossings() const;

    // Half width of a patch at the boundary in front of segment `boundary`
    // (the narrower side, so patches come to a point at both ends), and its
    // centre.
    float patch_width_at(int boundary) const;
    float patch_center_at(int boundary) const;

    // The patch a car at lateral position x with the given half width (both
    // in road half-widths) touches on the segment at z, or Patch::None.
    Patch patch_under(float z, float x, float half_width) const;

    std::vector<Branch> branches;

    // Makes route `route` (0 left, 1 right) of branch `index` the active one.
    void choose_branch(size_t index, int route);

    // Lateral position of the other route's road relative to the active one,
    // in road half-widths, at the boundary in front of segment `boundary`;
    // NaN outside forks. Where the routes part and meet it follows both
    // routes' curves; in between the other road runs alongside, apart.
    float branch_offset(int boundary) const;
    // How fast that offset changes there: the other road's heading relative
    // to the active one, in world units sideways per segment.
    float branch_slope(int boundary) const;

    // The branch whose routes contain segment `index`, or -1.
    int branch_at(int index) const;

    // At a fork, the inactive route's segment beside segment `index` of the
    // active one, or nullptr.
    const Segment* other_route_segment(int index) const;

    // Recomputes the offsets of the inactive routes; finish() and
    // choose_branch() do this.
    void update_branch_offsets();
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

// The tracks to choose from: 0 is the demo track, "SMALL WORLD", one zone
// per region; 1 "GRAND TOUR", far longer, through cities and countryside
// in every country.
constexpr int track_count = 2;
const char* track_name(int index);
Track build_track(int index);

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
