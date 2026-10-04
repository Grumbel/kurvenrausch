// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "track.hpp"

#include <algorithm>
#include <cassert>
#include <functional>
#include <cmath>
#include <cstdint>
#include <limits>

namespace racer {

const SceneryInfo& scenery_info(Scenery kind) {
    static const SceneryInfo infos[] = {
        //                width   solid  centered mirrorable
        /* Palm      */ {2000.f, true,  false, true},
        /* Tree      */ {1700.f, true,  false, true},
        /* Bush      */ { 900.f, false, false, true},
        /* Boulder   */ {1000.f, true,  false, true},
        /* Billboard */ {2200.f, true,  false, false},
        /* Gantry    */ {5200.f, false, true,  false},
        /* Fir       */ {1500.f, true,  false, true},
        /* SnowFir   */ {1500.f, true,  false, true},
        /* Cactus    */ { 800.f, true,  false, true},
        /* Mesa      */ {9000.f, true,  false, true},
        /* Cypress   */ { 700.f, true,  false, true},
        /* Chalet    */ {2600.f, true,  false, true},
        /* RedRock   */ {1100.f, true,  false, true},
        /* Pole      */ { 300.f, true,  false, false},
        /* DryShrub  */ { 700.f, false, false, true},
        /* BillboardUs*/{2200.f, true,  false, false},
        /* GasStation*/ {4800.f, true,  false, false},
        /* FuelPump  */ { 360.f, true,  false, false},
        /* FuelSign  */ { 700.f, true,  false, false},
        /* Victorian */ {2600.f, true,  false, true},
        /* VictorianB*/ {2600.f, true,  false, true},
        /* VictorianC*/ {2600.f, true,  false, true},
        /* StreetLamp*/ { 260.f, true,  false, false},
        /* SignLeft  */ { 900.f, true,  false, false},
        /* SignRight */ { 900.f, true,  false, false},
        /* Hedge     */ {1400.f, true,  false, true},
        /* StoneWall */ {1400.f, true,  false, true},
        /* PhoneBox  */ { 420.f, true,  false, false},
        /* Windmill  */ {3000.f, true,  false, true},
        /* Tulips    */ {2400.f, false, false, true},
        /* CherryTree*/ {1700.f, true,  false, true},
        /* Torii     */ {1500.f, true,  false, false},
        /* StoneLantern*/{ 320.f, true, false, false},
        /* Showroom  */ {4800.f, true,  false, false},
        /* DealerSign*/ { 700.f, true,  false, false},
        /* DatePalm  */ {1600.f, true,  false, true},
        /* Pyramid   */ {14000.f, true, false, false},
        /* Acacia    */ {2600.f, true,  false, true},
        /* Giraffe   */ { 900.f, true,  false, true},
        /* TermiteMound*/{ 500.f, true, false, true},
        /* Banyan    */ {3000.f, true,  false, true},
        /* Temple    */ {2600.f, true,  false, false},
        /* Cow       */ { 800.f, true,  false, true},
        /* Maple     */ {1700.f, true,  false, true},
        /* HanokGate */ {1800.f, true,  false, false},
        /* GumTree   */ {1600.f, true,  false, true},
        /* KangarooSign*/{ 500.f, true, false, false},
        /* Uluru     */ {16000.f, true, false, false},
        /* JungleTree*/ {2200.f, true,  false, true},
        /* Banana    */ { 900.f, true,  false, true},
        /* CarWash   */ {4800.f, true,  false, false},
        /* WashSign  */ { 700.f, true,  false, false},
        /* Motel     */ {4800.f, true,  false, false},
        /* MotelSign */ { 700.f, true,  false, false},
        /* Hospital  */ {4800.f, true,  false, false},
        /* HospitalSign*/{ 700.f, true, false, false},
        /* Truckstop */ {4800.f, true,  false, false},
        /* TruckSign */ { 700.f, true,  false, false},
        /* SportsShowroom*/{4800.f, true, false, false},
        /* SportsSign*/ { 700.f, true,  false, false},
        /* CrossingSign*/{ 500.f, true, false, false},
        /* TunnelPortal*/{16000.f, false, true, false},
        /* BridgeTruss*/{ 5200.f, false, true,  false},
        /* Overpass  */ { 6400.f, false, true,  false},
        /* ChemicalPlant*/{4800.f, true, false, false},
        /* ChemicalSign*/{ 700.f, true, false, false},
        /* Townhouse */ {2400.f, true,  false, true},
        /* TownhouseB*/ {2400.f, true,  false, true},
        /* Shop      */ {2600.f, true,  false, false},
        /* Apartment */ {3000.f, true,  false, true},
        /* Tower     */ {3200.f, true,  false, true},
        /* FlatHouse */ {2400.f, true,  false, true},
    };
    static_assert(sizeof(infos) / sizeof(infos[0]) == static_cast<size_t>(Scenery::Count),
                  "scenery_info() needs an entry for every Scenery kind");
    return infos[static_cast<int>(kind)];
}

float Track::wrap(float z) const {
    const float len = length();
    z = std::fmod(z, len);
    return z < 0.f ? z + len : z;
}

int Track::index_at(float z) const {
    const int n = static_cast<int>(segments.size());
    const int idx = static_cast<int>(std::floor(wrap(z) / segment_length));
    return idx >= n ? n - 1 : idx; // guards float rounding at the very end
}

const Segment* Track::other_route_segment(int index) const {
    const int n = static_cast<int>(segments.size());
    index = ((index % n) + n) % n;
    const int b = branch_at(index);
    if (b < 0) return nullptr;
    const Branch& br = branches[static_cast<size_t>(b)];
    return &br.routes[1 - br.active][static_cast<size_t>(index - br.fork)];
}

const Segment& Track::segment(int index) const {
    const int n = static_cast<int>(segments.size());
    return segments[static_cast<size_t>(((index % n) + n) % n)];
}

float Track::height_at(float z) const {
    z = wrap(z);
    const Segment& seg = segment_at(z);
    const float t = std::fmod(z, segment_length) / segment_length;
    return seg.y1 + (seg.y2 - seg.y1) * t;
}

const RoadTheme& Track::look(int segment) const {
    const int n = static_cast<int>(looks.size());
    return looks[static_cast<size_t>(((segment % n) + n) % n)];
}

int Track::zone_number_at(float z) const {
    return zone_index[static_cast<size_t>(index_at(z))];
}

const Zone& Track::zone_at(float z) const {
    return zones[static_cast<size_t>(zone_number_at(z))];
}

float Track::half_width(int boundary) const {
    return looks.empty() ? road_width : road_width * look(boundary).road_scale;
}

float Track::half_width_at(float z) const {
    const int i = index_at(z);
    const float t = std::fmod(wrap(z), segment_length) / segment_length;
    return half_width(i) + (half_width(i + 1) - half_width(i)) * t;
}

float Track::forecourt_at(int boundary) const {
    return std::min(segment(boundary).forecourt, segment(boundary - 1).forecourt);
}

bool Track::on_forecourt(float z, float x) const {
    const Segment& s = segment_at(z);
    const float out = x * static_cast<float>(s.court_side);
    return s.forecourt > 1.f && out > 1.f && out < s.forecourt;
}

namespace {

struct LotNames {
    const char* name;
    const char* keyword;
};

const LotNames& lot_names(Lot kind) {
    static const LotNames names[] = {
        {"GAS STATION", "gas"},
        {"CAR DEALER", "dealer"},
        {"CAR WASH", "wash"},
        {"MOTEL", "motel"},
        {"HOSPITAL", "hospital"},
        {"TRUCK STOP", "truckstop"},
        {"SPORTS CARS", "sports"},
        {"CHEMICAL PLANT", "chemical"},
    };
    static_assert(sizeof(names) / sizeof(names[0]) == static_cast<size_t>(lot_kinds), "every lot needs its names");
    return names[static_cast<size_t>(kind)];
}

} // namespace

const char* lot_name(Lot kind) { return lot_names(kind).name; }
const char* lot_keyword(Lot kind) { return lot_names(kind).keyword; }

std::vector<int> Track::crossings() const {
    std::vector<int> found;
    for (int i = 0; i < static_cast<int>(segments.size()); ++i) {
        if (segments[static_cast<size_t>(i)].rails) found.push_back(i);
    }
    return found;
}

std::vector<int> Track::lots(Lot kind) const {
    std::vector<int> starts;
    const int n = static_cast<int>(segments.size());
    for (int i = 0; i < n; ++i) {
        const Segment& s = segment(i);
        if (s.lot == kind && s.forecourt >= forecourt_width && segment(i - 1).forecourt < forecourt_width)
            starts.push_back(i);
    }
    return starts;
}

void Track::choose_branch(size_t index, int route) {
    Branch& br = branches.at(index);
    if (route == br.active) return;
    // Keep the active route's segments, which may have been changed since
    // (scenery is planted after building), then put the other one in.
    std::copy(segments.begin() + br.fork, segments.begin() + br.end(), br.routes[br.active].begin());
    std::copy(br.routes[route].begin(), br.routes[route].end(), segments.begin() + br.fork);
    br.active = route;
    update_branch_offsets();
}

void Track::update_branch_offsets() {
    const int n = static_cast<int>(segments.size());
    branch_offsets.assign(static_cast<size_t>(n) + 1, std::numeric_limits<float>::quiet_NaN());
    branch_slopes.assign(static_cast<size_t>(n) + 1, 0.f);
    for (const Branch& br : branches) {
        const std::vector<Segment>& other = br.routes[1 - br.active];
        // The same recurrence as the renderer's curve accumulation, on the
        // difference of the two routes' curves: offset and its slope.
        auto walk = [&](int from, int to, float offset) {
            float slope = 0.f;
            for (int i = from; i <= to; ++i) {
                branch_offsets[static_cast<size_t>(i)] = offset / half_width(i);
                branch_slopes[static_cast<size_t>(i)] = slope;
                if (i == to) break;
                offset += slope;
                slope += other[static_cast<size_t>(i - br.fork)].curve - segment(i).curve;
            }
            return offset;
        };
        // Where they part, from together to apart; where they meet, from the
        // same distance apart back together; in between the other road runs
        // alongside, that far apart, so it never appears or vanishes.
        const float apart = walk(br.fork, br.fork + br.bend, 0.f);
        for (int i = br.fork + br.bend; i < br.end() - br.bend; ++i) {
            branch_offsets[static_cast<size_t>(i)] = apart / half_width(i);
        }
        walk(br.end() - br.bend, br.end(), apart);
    }
}

float Track::branch_slope(int boundary) const {
    if (branch_slopes.empty()) return 0.f;
    const int n = static_cast<int>(segments.size());
    return branch_slopes[static_cast<size_t>(((boundary % n) + n) % n)];
}

float Track::branch_offset(int boundary) const {
    if (branch_offsets.empty()) return std::numeric_limits<float>::quiet_NaN();
    const int n = static_cast<int>(segments.size());
    return branch_offsets[static_cast<size_t>(((boundary % n) + n) % n)];
}

int Track::branch_at(int index) const {
    for (size_t i = 0; i < branches.size(); ++i) {
        if (index >= branches[i].fork && index < branches[i].end()) return static_cast<int>(i);
    }
    return -1;
}

float Track::patch_width_at(int boundary) const {
    return std::min(segment(boundary).patch_w, segment(boundary - 1).patch_w);
}

float Track::patch_center_at(int boundary) const {
    return segment(boundary).patch_w > 0.f ? segment(boundary).patch_x : segment(boundary - 1).patch_x;
}

Patch Track::patch_under(float z, float x, float half_width) const {
    const Segment& s = segment_at(z);
    const bool touching = s.patch_w > 0.f && std::abs(x - s.patch_x) < s.patch_w + half_width;
    return touching ? s.patch : Patch::None;
}

float Track::edge_height(int boundary, int side) const {
    const Segment& cur = segment(boundary);
    const Segment& prev = segment(boundary - 1);
    const Edge e_cur = side < 0 ? cur.left : cur.right;
    const Edge e_prev = side < 0 ? prev.left : prev.right;
    if (e_cur == Edge::Rail || e_prev == Edge::Rail) return rail_height;
    if (e_cur != Edge::Cliff && e_prev != Edge::Cliff) return 0.f;

    // Cliff: smooth variation along the road, zero where a run begins or ends.
    const float fade = std::min(e_cur == Edge::Cliff ? cur.edge_fade : 0.f,
                                e_prev == Edge::Cliff ? prev.edge_fade : 0.f);
    const float i = static_cast<float>(boundary);
    const float variation = 0.78f + 0.2f * std::sin(i * 0.13f) + 0.1f * std::sin(i * 0.37f + 1.3f);
    return cliff_height * variation * fade;
}

std::vector<MapPoint> track_map(const Track& track, float bend_scale) {
    const int n = static_cast<int>(track.segments.size());
    std::vector<MapPoint> pts(static_cast<size_t>(n));
    if (n == 0) return pts;
    // Heading change per segment for a bend: the lateral slope changes by
    // curve / segment_length per segment.
    const double k = static_cast<double>(bend_scale) / static_cast<double>(track.segment_length);
    double total = 0.0;
    for (const Segment& s : track.segments) total += static_cast<double>(s.curve) * k;
    const double turn = total >= 0.0 ? 2.0 * 3.14159265358979 : -2.0 * 3.14159265358979;
    const double extra = (turn - total) / n;

    std::vector<double> xs(static_cast<size_t>(n) + 1), ys(static_cast<size_t>(n) + 1);
    double heading = 0.0, x = 0.0, y = 0.0;
    for (int i = 0; i <= n; ++i) {
        xs[static_cast<size_t>(i)] = x;
        ys[static_cast<size_t>(i)] = y;
        if (i == n) break;
        heading += static_cast<double>(track.segments[static_cast<size_t>(i)].curve) * k + extra;
        x += std::sin(heading);
        y -= std::cos(heading);
    }
    // Close the loop.
    double min_x = 1e300, max_x = -1e300, min_y = 1e300, max_y = -1e300;
    for (int i = 0; i < n; ++i) {
        const double f = static_cast<double>(i) / n;
        xs[static_cast<size_t>(i)] -= xs[static_cast<size_t>(n)] * f;
        ys[static_cast<size_t>(i)] -= ys[static_cast<size_t>(n)] * f;
        min_x = std::min(min_x, xs[static_cast<size_t>(i)]);
        max_x = std::max(max_x, xs[static_cast<size_t>(i)]);
        min_y = std::min(min_y, ys[static_cast<size_t>(i)]);
        max_y = std::max(max_y, ys[static_cast<size_t>(i)]);
    }
    const double size = std::max({max_x - min_x, max_y - min_y, 1e-9});
    const double ox = (size - (max_x - min_x)) / 2.0, oy = (size - (max_y - min_y)) / 2.0;
    for (int i = 0; i < n; ++i) {
        pts[static_cast<size_t>(i)] = {static_cast<float>((xs[static_cast<size_t>(i)] - min_x + ox) / size),
                                       static_cast<float>((ys[static_cast<size_t>(i)] - min_y + oy) / size)};
    }
    return pts;
}

bool crossed_line_forward(float prev_z, float z, float line_z, float length) {
    const auto wrap = [length](float v) {
        v = std::fmod(v, length);
        return v < 0.f ? v + length : v;
    };
    // The signed step along the loop, folded into [-length/2, length/2).
    const float step = wrap(z - prev_z + length / 2.f) - length / 2.f;
    const float before = wrap(prev_z - line_z); // distance already past the line
    return step > 0.f && before + step >= length;
}

float barrier_limit(const Segment& seg, int side, float car_half_width) {
    const Edge e = side < 0 ? seg.left : seg.right;
    if (e == Edge::Rail) return rail_offset - car_half_width;
    if (e == Edge::Cliff && seg.edge_fade > 0.35f) return cliff_offset - car_half_width;
    return std::numeric_limits<float>::infinity();
}

RoadTheme mix_themes(const RoadTheme& a, const RoadTheme& b, float t) {
    // Tripwire: when a field is added to RoadTheme this changes, as a reminder
    // to blend it below and to update the expected size.
    static_assert(sizeof(RoadTheme) == 192, "RoadTheme changed: update mix_themes()");

    RoadTheme r = t < 0.5f ? a : b; // discrete fields come from the nearer theme
    const auto c = [t](Color x, Color y) { return blend(x, y, t); };
    const auto f = [t](float x, float y) { return x + (y - x) * t; };

    r.sky_top = c(a.sky_top, b.sky_top);
    r.sky_horizon = c(a.sky_horizon, b.sky_horizon);
    r.fog = c(a.fog, b.fog);
    for (int i = 0; i < 2; ++i) {
        r.grass[i] = c(a.grass[i], b.grass[i]);
        r.road[i] = c(a.road[i], b.road[i]);
        r.rumble[i] = c(a.rumble[i], b.rumble[i]);
        r.checker[i] = c(a.checker[i], b.checker[i]);
    }
    r.lane = c(a.lane, b.lane);
    for (int i = 0; i < 3; ++i) r.cloud[i] = c(a.cloud[i], b.cloud[i]);
    r.mountain_lit = c(a.mountain_lit, b.mountain_lit);
    r.mountain_shade = c(a.mountain_shade, b.mountain_shade);
    r.snow = c(a.snow, b.snow);
    r.hill_lit = c(a.hill_lit, b.hill_lit);
    r.hill_shade = c(a.hill_shade, b.hill_shade);
    r.fog_density = f(a.fog_density, b.fog_density);
    for (int i = 0; i < 3; ++i) r.rock[i] = c(a.rock[i], b.rock[i]);
    r.cap = c(a.cap, b.cap);
    r.cap_amount = f(a.cap_amount, b.cap_amount);
    for (int i = 0; i < 2; ++i) {
        r.rail[i] = c(a.rail[i], b.rail[i]);
        r.beyond[i] = c(a.beyond[i], b.beyond[i]);
    }
    r.cloud_tint = c(a.cloud_tint, b.cloud_tint);
    r.cloud_tint_amount = f(a.cloud_tint_amount, b.cloud_tint_amount);
    r.sun = c(a.sun, b.sun);
    r.sun_amount = f(a.sun_amount, b.sun_amount);
    r.stars = f(a.stars, b.stars);
    r.haze = f(a.haze, b.haze);
    r.night_glow = f(a.night_glow, b.night_glow);
    r.mountain_scale = f(a.mountain_scale, b.mountain_scale);
    r.hill_scale = f(a.hill_scale, b.hill_scale);
    r.snow_line = f(a.snow_line, b.snow_line);
    r.rain = f(a.rain, b.rain);
    r.snowfall = f(a.snowfall, b.snowfall);
    r.showers = f(a.showers, b.showers);
    r.grip = f(a.grip, b.grip);
    r.road_scale = f(a.road_scale, b.road_scale);
    r.center_line = c(a.center_line, b.center_line);
    // lanes, us_markings and left_hand are discrete: they stay those of the
    // nearer theme.
    return r;
}

void Track::finish(int transition_segments) {
    assert(!zones.empty() && zones.front().first_segment == 0);
    update_branch_offsets();
    const int n = static_cast<int>(segments.size());
    const int zone_count = static_cast<int>(zones.size());
    const auto zone_end = [&](int k) { return k + 1 < zone_count ? zones[static_cast<size_t>(k) + 1].first_segment : n; };

    zone_index.assign(static_cast<size_t>(n), 0);
    looks.resize(static_cast<size_t>(n));
    int shortest = n;
    for (int k = 0; k < zone_count; ++k) {
        const int first = zones[static_cast<size_t>(k)].first_segment;
        shortest = std::min(shortest, zone_end(k) - first);
        for (int i = first; i < zone_end(k); ++i) {
            zone_index[static_cast<size_t>(i)] = k;
            looks[static_cast<size_t>(i)] = zones[static_cast<size_t>(k)].theme;
        }
    }
    if (zone_count < 2) return;

    // Each boundary gets a smooth fade, including the one across the lap seam.
    const int length = std::max(2, std::min(transition_segments, shortest));
    const int half = length / 2;
    for (int k = 0; k < zone_count; ++k) {
        const RoadTheme& from = zones[static_cast<size_t>(k)].theme;
        const RoadTheme& to = zones[static_cast<size_t>((k + 1) % zone_count)].theme;
        const int boundary = zone_end(k); // n for the last zone, i.e. the seam
        for (int i = 0; i < length; ++i) {
            const int s = (((boundary - half + i) % n) + n) % n;
            const float x = (static_cast<float>(i) + 0.5f) / static_cast<float>(length);
            const float eased = x * x * (3.f - 2.f * x); // smoothstep
            looks[static_cast<size_t>(s)] = mix_themes(from, to, eased);
        }
    }
}

namespace {

float ease_in(float a, float b, float t) { return a + (b - a) * t * t; }
float ease_in_out(float a, float b, float t) {
    return a + (b - a) * (0.5f - 0.5f * std::cos(t * PI));
}

// Small deterministic generator so the scenery is the same on every run.
class Rng {
public:
    explicit Rng(uint32_t seed) : state_(seed) {}
    float next() { // [0, 1)
        state_ = state_ * 1664525u + 1013904223u;
        return static_cast<float>(state_ >> 8) / 16777216.f;
    }
    float range(float a, float b) { return a + (b - a) * next(); }
    bool chance(float p) { return next() < p; }

private:
    uint32_t state_;
};

// Road length / curve / hill vocabulary in the classic style.
namespace Len { constexpr int Short = 25, Medium = 50, Long = 100; }
namespace Bend { constexpr float None = 0.f, Easy = 2.f, Medium = 4.f, Hard = 6.f; }
namespace Hill { constexpr float None = 0.f, Low = 20.f, Medium = 40.f, High = 60.f; }

class TrackBuilder {
public:
    explicit TrackBuilder(Track& t) : t_(t) {}

    // A road section that eases into a curve, holds it, and eases out, while
    // smoothly changing height by `hill` segment lengths over its length.
    void road(int enter, int hold, int leave, float curve, float hill) {
        const float start_y = last_y();
        const float end_y = start_y + hill * t_.segment_length;
        const int total = enter + hold + leave;
        int n = 0;
        auto height = [&] { return ease_in_out(start_y, end_y, static_cast<float>(++n) / total); };
        for (int i = 0; i < enter; ++i) add(ease_in(0.f, curve, static_cast<float>(i) / enter), height());
        for (int i = 0; i < hold; ++i) add(curve, height());
        for (int i = 0; i < leave; ++i) add(ease_in_out(curve, 0.f, static_cast<float>(i) / leave), height());
    }

    void straight(int len) { road(len, len, len, Bend::None, Hill::None); }
    void hill(int len, float height) { road(len, len, len, Bend::None, height); }
    void curve(int len, float bend, float height) { road(len, len, len, bend, height); }

    void s_curves() {
        road(Len::Medium, Len::Medium, Len::Medium, -Bend::Easy, Hill::None);
        road(Len::Medium, Len::Medium, Len::Medium, Bend::Medium, Hill::Medium);
        road(Len::Medium, Len::Medium, Len::Medium, Bend::Easy, -Hill::Low);
        road(Len::Medium, Len::Medium, Len::Medium, -Bend::Easy, Hill::Medium);
        road(Len::Medium, Len::Medium, Len::Medium, -Bend::Medium, -Hill::Medium);
    }

    void low_rolling_hills() {
        const int n = Len::Short;
        road(n, n, n, 0.f, Hill::Low / 2.f);
        road(n, n, n, 0.f, -Hill::Low);
        road(n, n, n, Bend::Easy, Hill::Low);
        road(n, n, n, 0.f, 0.f);
        road(n, n, n, -Bend::Easy, Hill::Low / 2.f);
        road(n, n, n, 0.f, 0.f);
    }

    void bumps() {
        road(10, 10, 10, 0.f, 5.f);
        road(10, 10, 10, 0.f, -2.f);
        road(10, 10, 10, 0.f, -5.f);
        road(10, 10, 10, 0.f, 8.f);
        road(10, 10, 10, 0.f, 5.f);
        road(10, 10, 10, 0.f, -7.f);
        road(10, 10, 10, 0.f, 5.f);
        road(10, 10, 10, 0.f, -2.f);
    }

    // A straight constant grade (rise per length), the way San Francisco's
    // streets run up and down the hills: the grade changes abruptly where it
    // meets the next one, and a crest taken fast throws the car into the air.
    void slope(int len, float grade, float curve = Bend::None) {
        for (int i = 0; i < len; ++i) add(curve, last_y() + grade * t_.segment_length);
    }

    // Brings the height back to zero so the loop closes seamlessly.
    void downhill_to_end(int len) {
        road(len, len, len, -Bend::Easy, -last_y() / t_.segment_length);
    }

    int size() const { return static_cast<int>(t_.segments.size()); }

    // Starts a new zone at the current end of the track.
    void begin_zone(Zone zone) {
        zone.first_segment = size();
        t_.zones.push_back(std::move(zone));
    }

    // Puts edge features along segments [from, to). Cliffs fade in and out over
    // the first and last few segments of the run.
    void mark(int from, int to, Edge left, Edge right) {
        constexpr float ramp = 12.f;
        for (int i = from; i < to && i < size(); ++i) {
            Segment& seg = t_.segments[static_cast<size_t>(i)];
            seg.left = left;
            seg.right = right;
            seg.edge_fade = std::clamp(std::min(static_cast<float>(i - from + 1), static_cast<float>(to - i)) / ramp,
                                       0.f, 1.f);
        }
    }

    // A gas station on the right: a flat straight with the forecourt beside
    // it, tapering in and out, a sign ahead of it, two pumps and the shop.
    void gas_station() { forecourt_lot(Lot::Gas); }

    // A car dealer, laid out like a gas station: a showroom beyond the
    // forecourt, a sign ahead of it.
    void car_dealer() { forecourt_lot(Lot::Dealer); }
    void sports_dealer() { forecourt_lot(Lot::SportsDealer); }

    // A car wash, laid out the same way: the wash bay beyond the forecourt.
    void car_wash() { forecourt_lot(Lot::Wash); }

    // A motel and a hospital, laid out the same way.
    void motel() { forecourt_lot(Lot::Motel); }
    void hospital() { forecourt_lot(Lot::Hospital); }
    void truckstop() { forecourt_lot(Lot::Truckstop); }
    // A chemical plant, where nitro is to be had.
    void chemical_plant() { forecourt_lot(Lot::Chemical); }

    void forecourt_lot(Lot kind) {
        const int from = size();
        road(8, 40, 8, Bend::None, Hill::None);
        constexpr int start = 6, taper = 4, length = 44;
        // On the side traffic keeps to.
        const int8_t side = !t_.zones.empty() && t_.zones.back().theme.left_hand ? -1 : 1;
        for (int i = 0; i < length; ++i) {
            const float in = std::min(static_cast<float>(i + 1), static_cast<float>(length - i)) / taper;
            Segment& seg = t_.segments[static_cast<size_t>(from + start + i)];
            seg.forecourt = 1.f + (forecourt_width - 1.f) * std::min(1.f, in);
            seg.court_side = side;
            seg.lot = kind;
        }
        const auto scenery = [&](int index, Scenery what, float offset) {
            this->scenery(index, what, offset * static_cast<float>(side));
        };
        // The sign ahead, the building beyond the forecourt.
        switch (kind) {
            case Lot::Gas:
                scenery(from + 1, Scenery::FuelSign, 1.25f);
                scenery(from + start + 16, Scenery::FuelPump, 1.75f);
                scenery(from + start + 24, Scenery::FuelPump, 1.75f);
                scenery(from + start + 30, Scenery::GasStation, forecourt_width + 0.1f);
                break;
            case Lot::Dealer:
                scenery(from + 1, Scenery::DealerSign, 1.25f);
                scenery(from + start + 26, Scenery::Showroom, forecourt_width + 0.1f);
                break;
            case Lot::Wash:
                scenery(from + 1, Scenery::WashSign, 1.25f);
                scenery(from + start + 26, Scenery::CarWash, forecourt_width + 0.1f);
                break;
            case Lot::Motel:
                scenery(from + 1, Scenery::MotelSign, 1.25f);
                scenery(from + start + 26, Scenery::Motel, forecourt_width + 0.1f);
                break;
            case Lot::Hospital:
                scenery(from + 1, Scenery::HospitalSign, 1.25f);
                scenery(from + start + 26, Scenery::Hospital, forecourt_width + 0.1f);
                break;
            case Lot::Truckstop:
                scenery(from + 1, Scenery::TruckSign, 1.25f);
                scenery(from + start + 26, Scenery::Truckstop, forecourt_width + 0.1f);
                break;
            case Lot::SportsDealer:
                scenery(from + 1, Scenery::SportsSign, 1.25f);
                scenery(from + start + 26, Scenery::SportsShowroom, forecourt_width + 0.1f);
                break;
            case Lot::Chemical:
                scenery(from + 1, Scenery::ChemicalSign, 1.25f);
                scenery(from + start + 26, Scenery::ChemicalPlant, forecourt_width + 0.1f);
                break;
        }
    }

    // A fork: two routes, `left` and `right` building their middle parts.
    // Each first bends away from the other (an S-bend, flat, which leaves
    // them side by side, apart), then follows its own course, then bends
    // back to meet the other. The shorter is padded so both are equally long
    // and end at the starting height. Signs before the fork point the way.
    void fork(std::string left_name, std::string right_name,
              const std::function<void()>& left, const std::function<void()>& right) {
        // The routes part (and meet) in an S-bend whose curvature follows a
        // full sine, so it eases in, turns over and eases out without a jolt:
        // over `bend` segments each route moves apart by `apart` world units.
        constexpr int bend = 70;
        constexpr float apart = 3750.f;
        const float peak = apart * 2.f * PI / static_cast<float>(bend * bend);
        const int start = size();
        const float y0 = last_y();
        scenery(start - 10, Scenery::SignLeft, -1.2f);
        scenery(start - 10, Scenery::SignRight, 1.2f);

        auto s_bend = [&](float side) { // side: the way this route moves
            for (int i = 0; i < bend; ++i) {
                const float phase = 2.f * PI * (static_cast<float>(i) + 0.5f) / static_cast<float>(bend);
                add(side * peak * std::sin(phase), y0);
                t_.segments.back().branch_bend = true;
            }
        };
        auto part = [&](float side) { s_bend(side); };
        std::vector<Segment> routes[2];
        int middle[2] = {0, 0};
        const std::function<void()>* builds[2] = {&left, &right};
        for (int r = 0; r < 2; ++r) {
            t_.segments.resize(static_cast<size_t>(start));
            part(r == 0 ? -1.f : 1.f);
            const int before = size();
            (*builds[r])();
            middle[r] = size() - before;
            routes[r].assign(t_.segments.begin() + start, t_.segments.end());
        }
        const int longest = std::max(middle[0], middle[1]) + 30;
        for (int r = 0; r < 2; ++r) {
            t_.segments.resize(static_cast<size_t>(start));
            t_.segments.insert(t_.segments.end(), routes[r].begin(), routes[r].end());
            // Pad to the same length, easing back to the starting height.
            const int pad = longest - middle[r];
            road(pad / 3, pad - 2 * (pad / 3), pad / 3, Bend::None, (y0 - last_y()) / t_.segment_length);
            const float side = r == 0 ? -1.f : 1.f;
            s_bend(-side); // and back towards the other
            for (int i = start; i < size(); ++i) t_.segments[static_cast<size_t>(i)].facing_branch = static_cast<int8_t>(-side);
            routes[r].assign(t_.segments.begin() + start, t_.segments.end());
        }
        Branch br;
        br.fork = start;
        br.length = static_cast<int>(routes[0].size());
        br.bend = bend;
        br.names[0] = std::move(left_name);
        br.names[1] = std::move(right_name);
        br.routes[0] = std::move(routes[0]);
        br.routes[1] = std::move(routes[1]);
        br.active = 1; // the right route is in the track now
        t_.branches.push_back(std::move(br));
    }

    // A level crossing: a straight, the railway across it in the middle,
    // a crossbuck on each side before it.
    void level_crossing() {
        const int from = size();
        straight(Len::Short);
        const int at = from + Len::Short + Len::Short / 2;
        t_.segments[static_cast<size_t>(at)].rails = true;
        scenery(at - 3, Scenery::CrossingSign, 1.15f);
        scenery(at - 3, Scenery::CrossingSign, -1.15f);
    }

    // A tunnel through the hill: walls and a ceiling, a portal at each end.
    void tunnel(int len, float bend, float hill) {
        const int from = size();
        road(len, len, len, bend, hill);
        for (int i = from; i < size(); ++i) {
            Segment& s = t_.segments[static_cast<size_t>(i)];
            s.tunnel = true;
            s.left = s.right = Edge::None;
        }
        scenery(from, Scenery::TunnelPortal, 0.f);
        scenery(size() - 1, Scenery::TunnelPortal, 0.f); // the way out, seen from inside
    }

    // A bridge over a river: railings and the water beyond them, a truss
    // frame over the road every few segments.
    void bridge(int len) {
        const int from = size();
        straight(len);
        mark(from, size(), Edge::Rail, Edge::Rail);
        for (int i = from + 4; i < size() - 4; i += 6) scenery(i, Scenery::BridgeTruss, 0.f);
    }

    // A road bridge crossing over this one.
    void overpass() {
        const int from = size();
        straight(Len::Short / 2);
        scenery(from + Len::Short * 3 / 4, Scenery::Overpass, 0.f);
    }

    void scenery(int index, Scenery kind, float offset) {
        if (index < 0 || index >= static_cast<int>(t_.segments.size())) return;
        t_.segments[static_cast<size_t>(index)].scenery.push_back({kind, offset});
    }

private:
    float last_y() const { return t_.segments.empty() ? 0.f : t_.segments.back().y2; }

    void add(float curve, float y) {
        Segment s;
        const int index = static_cast<int>(t_.segments.size());
        s.curve = curve;
        s.y1 = last_y();
        s.y2 = y;
        s.alt = (index / 3) % 2 == 0;
        t_.segments.push_back(std::move(s));
    }

    Track& t_;
};

} // namespace

// ---- The six zones of the demo route ---------------------------------------

Zone zone_france() {
    Zone z{"FRANCE", "COTE D'AZUR", RoadTheme{}, 0, Decor::Riviera};
    z.theme.sun_amount = 0.3f; // a small noon sun, the classic default look otherwise
    z.theme.showers = 0.30f;
    return z;
}

Zone zone_england() {
    Zone z{"ENGLAND", "COTSWOLDS", RoadTheme{}, 0, Decor::Country};
    RoadTheme& t = z.theme;
    t.sky_top = Color{0x6c, 0x8c, 0xb8};
    t.sky_horizon = Color{0xc8, 0xd4, 0xdc};
    t.fog = Color{0xc4, 0xcc, 0xd0};
    t.grass[0] = Color{0x58, 0xa0, 0x44};
    t.grass[1] = Color{0x4e, 0x94, 0x3c};
    t.road[0] = Color{0x58, 0x58, 0x5c};
    t.road[1] = Color{0x52, 0x52, 0x56};
    t.rumble[0] = Color{0xe0, 0xe0, 0xd8}; // a white edge line, no kerb stripes
    t.rumble[1] = Color{0xe0, 0xe0, 0xd8};
    t.mountain_lit = Color{0x88, 0xa0, 0x88};
    t.mountain_shade = Color{0x70, 0x88, 0x74};
    t.hill_lit = Color{0x64, 0xa0, 0x50};
    t.hill_shade = Color{0x54, 0x8c, 0x44};
    t.cloud_tint = Color{0xa8, 0xb0, 0xbc};
    t.cloud_tint_amount = 0.5f;
    t.fog_density = 7.f;
    t.haze = 0.4f;
    t.mountain_scale = 0.35f;
    t.hill_scale = 1.1f;
    t.snow_line = 1.0e9f;
    t.rain = 0.25f; // drizzle
    t.grip = 0.9f;
    t.lanes = 2;
    t.road_scale = 0.62f; // narrow country lanes
    z.theme.showers = 0.60f;
    z.theme.left_hand = true;
    z.theme.night_glow = 0.2f; // villages
    return z;
}

Zone zone_netherlands() {
    Zone z{"NETHERLANDS", "HOLLAND", RoadTheme{}, 0, Decor::Polder};
    RoadTheme& t = z.theme;
    t.sky_top = Color{0x3c, 0x78, 0xd0};
    t.sky_horizon = Color{0xd4, 0xe8, 0xf8};
    t.fog = Color{0xcc, 0xe0, 0xf0};
    t.grass[0] = Color{0x5c, 0xb0, 0x40};
    t.grass[1] = Color{0x52, 0xa4, 0x38};
    t.road[0] = Color{0x6c, 0x6c, 0x70};
    t.road[1] = Color{0x66, 0x66, 0x6a};
    t.beyond[0] = Color{0x4c, 0x7c, 0x9c}; // the canal
    t.beyond[1] = Color{0x50, 0x82, 0xa4};
    t.cloud_tint = Color{0xff, 0xff, 0xff};
    t.cloud_tint_amount = 0.f;
    t.fog_density = 3.f;
    t.haze = 0.3f;
    t.mountain_scale = 0.05f; // dead flat
    t.hill_scale = 0.15f;
    t.snow_line = 1.0e9f;
    t.rain = 0.1f;
    t.lanes = 2;
    z.theme.showers = 0.60f;
    z.theme.night_glow = 0.25f; // villages everywhere
    return z;
}

Zone zone_japan() {
    Zone z{"JAPAN", "FUJI", RoadTheme{}, 0, Decor::Sakura};
    RoadTheme& t = z.theme;
    t.sky_top = Color{0x5c, 0x94, 0xdc};
    t.sky_horizon = Color{0xf0, 0xdc, 0xe4};
    t.fog = Color{0xec, 0xdc, 0xe4};
    t.grass[0] = Color{0x6c, 0xa8, 0x50};
    t.grass[1] = Color{0x62, 0x9c, 0x48};
    t.road[0] = Color{0x60, 0x60, 0x66};
    t.road[1] = Color{0x5a, 0x5a, 0x60};
    t.rumble[0] = Color{0xf0, 0xf0, 0xf0};
    t.rumble[1] = Color{0xc8, 0x30, 0x30};
    t.mountain_lit = Color{0x7c, 0x90, 0xc4}; // the volcano
    t.mountain_shade = Color{0x5c, 0x70, 0xa8};
    t.hill_lit = Color{0x5c, 0x94, 0x4c};
    t.hill_shade = Color{0x4c, 0x80, 0x40};
    t.cloud_tint = Color{0xff, 0xf0, 0xf4};
    t.cloud_tint_amount = 0.3f;
    t.sun_amount = 0.5f;
    t.fog_density = 4.f;
    t.haze = 0.2f;
    t.mountain_scale = 1.9f;
    t.snow_line = 44.f;
    t.lanes = 2;
    t.road_scale = 0.75f;
    z.theme.showers = 0.50f;
    z.theme.left_hand = true;
    return z;
}

// A theme from a few colours; the rest as RoadTheme's defaults.
RoadTheme make_theme(Color sky_top, Color sky_horizon, Color fog, Color grass0, Color grass1, Color road,
                     Color mountain_lit, Color mountain_shade, Color hill_lit, Color hill_shade) {
    RoadTheme t;
    t.sky_top = sky_top;
    t.sky_horizon = sky_horizon;
    t.fog = fog;
    t.grass[0] = grass0;
    t.grass[1] = grass1;
    t.road[0] = road;
    t.road[1] = blend(road, Color{0, 0, 0}, 0.06f);
    t.mountain_lit = mountain_lit;
    t.mountain_shade = mountain_shade;
    t.hill_lit = hill_lit;
    t.hill_shade = hill_shade;
    t.snow_line = 1.0e9f;
    t.lanes = 2;
    return t;
}

Zone zone_egypt() {
    Zone z{"EGYPT", "NILE", make_theme({0x30, 0x7c, 0xd8}, {0xf4, 0xe4, 0xbc}, {0xf0, 0xe0, 0xb8},
                                       {0xe4, 0xc0, 0x7c}, {0xd8, 0xb4, 0x70}, {0x78, 0x70, 0x6c},
                                       {0xd4, 0xa8, 0x68}, {0xb4, 0x88, 0x54}, {0xdc, 0xb4, 0x74},
                                       {0xc8, 0xa0, 0x64}),
           0, Decor::Nile};
    RoadTheme& t = z.theme;
    t.rumble[0] = Color{0xf0, 0xe8, 0xd8};
    t.rumble[1] = Color{0x2c, 0x58, 0xa8};
    t.sun_amount = 0.8f;
    t.fog_density = 3.f;
    t.haze = 0.25f;
    t.mountain_scale = 0.2f;
    t.hill_scale = 0.4f;
    t.cloud_tint = Color{0xff, 0xf4, 0xe0};
    t.cloud_tint_amount = 0.2f;
    z.theme.showers = 0.00f;
    z.theme.night_glow = 0.05f; // the desert by the Nile
    return z;
}

Zone zone_kenya() {
    Zone z{"KENYA", "MASAI MARA", make_theme({0x2c, 0x50, 0xa0}, {0xf8, 0xb4, 0x64}, {0xf0, 0xb8, 0x80},
                                             {0xc8, 0xa8, 0x50}, {0xbc, 0x9c, 0x48}, {0x70, 0x60, 0x58},
                                             {0x9c, 0x88, 0xb0}, {0x7c, 0x6c, 0x98}, {0xa0, 0x88, 0x48},
                                             {0x88, 0x74, 0x40}),
           0, Decor::Savanna};
    RoadTheme& t = z.theme;
    t.rumble[0] = Color{0xb4, 0x5c, 0x30}; // red earth verges
    t.rumble[1] = Color{0x9c, 0x4c, 0x28};
    t.sun_amount = 1.f;
    t.fog_density = 3.f;
    t.haze = 0.2f;
    t.mountain_scale = 1.3f; // Kilimanjaro on the horizon
    t.snow_line = 40.f;
    t.cloud_tint = Color{0xf8, 0x9c, 0x5c};
    t.cloud_tint_amount = 0.5f;
    t.road_scale = 0.85f;
    z.theme.showers = 0.40f;
    z.theme.left_hand = true;
    z.theme.night_glow = 0.02f; // the savanna
    return z;
}

Zone zone_india() {
    Zone z{"INDIA", "RAJASTHAN", make_theme({0x6c, 0x9c, 0xc8}, {0xf0, 0xd8, 0xa8}, {0xe8, 0xd0, 0xa4},
                                            {0xb8, 0xa8, 0x5c}, {0xac, 0x9c, 0x54}, {0x6c, 0x64, 0x60},
                                            {0xc0, 0x98, 0x78}, {0xa0, 0x7c, 0x64}, {0x9c, 0x98, 0x50},
                                            {0x88, 0x84, 0x48}),
           0, Decor::Rajasthan};
    RoadTheme& t = z.theme;
    t.rumble[0] = Color{0xf0, 0xf0, 0xe0};
    t.rumble[1] = Color{0xe8, 0x9c, 0x20};
    t.sun_amount = 0.6f;
    t.fog_density = 6.f;
    t.haze = 0.5f; // hot and hazy
    t.mountain_scale = 0.4f;
    t.road_scale = 0.8f;
    z.theme.showers = 0.85f; // the monsoon
    z.theme.left_hand = true;
    return z;
}

Zone zone_korea() {
    Zone z{"KOREA", "SEORAKSAN", make_theme({0x3c, 0x7c, 0xd4}, {0xd4, 0xe4, 0xf0}, {0xd0, 0xdc, 0xe4},
                                            {0x9c, 0x8c, 0x4c}, {0x90, 0x80, 0x44}, {0x5c, 0x5c, 0x62},
                                            {0x88, 0x84, 0x90}, {0x6c, 0x6c, 0x78}, {0xb0, 0x5c, 0x30},
                                            {0x90, 0x4c, 0x2c}),
           0, Decor::Autumn};
    RoadTheme& t = z.theme;
    t.fog_density = 5.f;
    t.haze = 0.3f;
    t.mountain_scale = 1.4f; // rocky peaks
    t.hill_scale = 1.2f;
    t.sun_amount = 0.4f;
    z.theme.showers = 0.50f;
    return z;
}

Zone zone_australia() {
    Zone z{"AUSTRALIA", "OUTBACK", make_theme({0x24, 0x64, 0xd0}, {0xd8, 0xe4, 0xf0}, {0xe8, 0xc8, 0xa8},
                                              {0xc0, 0x60, 0x30}, {0xb4, 0x58, 0x2c}, {0x6c, 0x5c, 0x58},
                                              {0xb4, 0x6c, 0x48}, {0x94, 0x58, 0x3c}, {0xb8, 0x74, 0x44},
                                              {0xa0, 0x64, 0x3c}),
           0, Decor::Outback};
    RoadTheme& t = z.theme;
    t.rumble[0] = Color{0xf0, 0xf0, 0xe8};
    t.rumble[1] = Color{0xf0, 0xf0, 0xe8};
    t.sun_amount = 0.9f;
    t.fog_density = 2.5f;
    t.haze = 0.15f;
    t.mountain_scale = 0.3f;
    t.hill_scale = 0.3f;
    t.road_scale = 0.85f;
    z.theme.showers = 0.10f;
    z.theme.left_hand = true;
    z.theme.night_glow = 0.f; // the outback: no light for miles
    return z;
}

Zone zone_brazil() {
    Zone z{"BRAZIL", "AMAZONAS", make_theme({0x50, 0x68, 0x70}, {0xa8, 0xbc, 0xb0}, {0xa0, 0xb8, 0xa8},
                                            {0x2c, 0x78, 0x2c}, {0x28, 0x6c, 0x28}, {0x58, 0x54, 0x50},
                                            {0x3c, 0x64, 0x48}, {0x30, 0x54, 0x3c}, {0x30, 0x70, 0x30},
                                            {0x28, 0x60, 0x2c}),
           0, Decor::Jungle};
    RoadTheme& t = z.theme;
    t.rumble[0] = Color{0xe8, 0xe0, 0x40};
    t.rumble[1] = Color{0x28, 0x8c, 0x3c};
    t.cloud_tint = Color{0x70, 0x80, 0x80};
    t.cloud_tint_amount = 0.7f;
    t.fog_density = 9.f;
    t.haze = 0.55f;
    t.mountain_scale = 0.5f;
    t.hill_scale = 1.3f;
    t.rain = 0.9f; // tropical downpour
    t.grip = 0.82f;
    z.theme.showers = 0.10f;
    z.theme.night_glow = 0.03f; // the rainforest
    return z;
}

Zone zone_germany() {
    Zone z{"GERMANY", "SCHWARZWALD", RoadTheme{}, 0, Decor::Forest};
    RoadTheme& t = z.theme;
    t.sky_top = Color{0x56, 0x62, 0x78};
    t.sky_horizon = Color{0x9e, 0xaa, 0xb6};
    t.fog = Color{0x9e, 0xaa, 0xb6};
    t.grass[0] = Color{0x2c, 0x5c, 0x34};
    t.grass[1] = Color{0x28, 0x54, 0x30};
    t.road[0] = Color{0x4c, 0x50, 0x58};
    t.road[1] = Color{0x46, 0x4a, 0x52};
    t.lane = Color{0xd0, 0xd4, 0xd8};
    t.mountain_lit = Color{0x5c, 0x70, 0x74};
    t.mountain_shade = Color{0x48, 0x5c, 0x64};
    t.hill_lit = Color{0x30, 0x60, 0x3c};
    t.hill_shade = Color{0x28, 0x50, 0x34};
    t.cloud_tint = Color{0x50, 0x58, 0x66};
    t.cloud_tint_amount = 0.75f;
    t.fog_density = 9.f;
    t.haze = 0.5f;
    t.mountain_scale = 0.55f;
    t.snow_line = 1.0e9f;
    t.rain = 0.85f;
    t.grip = 0.8f;
    z.theme.showers = 0.15f;
    return z;
}

Zone zone_switzerland() {
    Zone z{"SWITZERLAND", "ALPS", RoadTheme{}, 0, Decor::Alpine};
    RoadTheme& t = z.theme;
    t.sky_top = Color{0x6c, 0x88, 0xb8};
    t.sky_horizon = Color{0xd0, 0xdc, 0xec};
    t.fog = Color{0xd8, 0xe2, 0xee};
    t.grass[0] = Color{0xec, 0xf2, 0xf8};
    t.grass[1] = Color{0xdc, 0xe6, 0xf0};
    t.road[0] = Color{0x5c, 0x60, 0x68};
    t.road[1] = Color{0x56, 0x5a, 0x62};
    t.cloud_tint = Color{0xb0, 0xbc, 0xd0};
    t.cloud_tint_amount = 0.45f;
    t.mountain_lit = Color{0x88, 0x98, 0xc0};
    t.mountain_shade = Color{0x68, 0x78, 0xa4};
    t.hill_lit = Color{0xc8, 0xd8, 0xe8};
    t.hill_shade = Color{0xa8, 0xbc, 0xd4};
    t.rock[0] = Color{0x4c, 0x50, 0x5c};
    t.rock[1] = Color{0x72, 0x78, 0x84};
    t.rock[2] = Color{0x9c, 0xa2, 0xae};
    t.cap = Color{0xf4, 0xf8, 0xff};
    t.cap_amount = 0.8f;
    t.beyond[0] = Color{0x9c, 0xb4, 0xd0}; // the valley far below, in haze
    t.beyond[1] = Color{0xa8, 0xbe, 0xd8};
    t.fog_density = 7.f;
    t.haze = 0.25f;
    t.mountain_scale = 1.6f;
    t.hill_scale = 0.7f;
    t.snow_line = 36.f;
    t.snowfall = 0.7f;
    t.grip = 0.7f;
    t.lanes = 2;
    z.theme.showers = 0.00f; // it snows harder instead
    z.theme.night_glow = 0.08f; // the mountains
    return z;
}

Zone zone_italy() {
    Zone z{"ITALY", "TOSCANA", RoadTheme{}, 0, Decor::Tuscany};
    RoadTheme& t = z.theme;
    t.sky_top = Color{0x38, 0x24, 0x68};
    t.sky_horizon = Color{0xf4, 0x9a, 0x54};
    t.fog = Color{0xec, 0xa4, 0x72};
    t.grass[0] = Color{0xa8, 0x98, 0x44};
    t.grass[1] = Color{0x98, 0x88, 0x3c};
    t.road[0] = Color{0x6e, 0x60, 0x64};
    t.road[1] = Color{0x66, 0x58, 0x5c};
    t.mountain_lit = Color{0xa4, 0x68, 0x7c};
    t.mountain_shade = Color{0x70, 0x46, 0x6c};
    t.hill_lit = Color{0x9c, 0x88, 0x44};
    t.hill_shade = Color{0x7c, 0x6c, 0x3a};
    t.cloud_tint = Color{0xf0, 0x84, 0x54};
    t.cloud_tint_amount = 0.6f;
    t.sun_amount = 1.f;
    t.fog_density = 4.f;
    t.haze = 0.2f;
    t.mountain_scale = 0.8f;
    t.snow_line = 1.0e9f;
    t.lanes = 2;
    z.theme.showers = 0.35f;
    return z;
}

Zone zone_arizona() {
    Zone z{"USA", "ARIZONA", RoadTheme{}, 0, Decor::Desert};
    RoadTheme& t = z.theme;
    t.sky_top = Color{0x1c, 0x5c, 0xc4};
    t.sky_horizon = Color{0xf2, 0xe2, 0xb4};
    t.fog = Color{0xf0, 0xdc, 0xb0};
    t.grass[0] = Color{0xe2, 0xb2, 0x72};
    t.grass[1] = Color{0xd6, 0xa6, 0x66};
    t.road[0] = Color{0x7a, 0x72, 0x6e};
    t.road[1] = Color{0x72, 0x6a, 0x66};
    t.rumble[0] = Color{0xf0, 0xe8, 0xd8};
    t.rumble[1] = Color{0xc8, 0x50, 0x30};
    t.mountain_lit = Color{0xc4, 0x78, 0x52};
    t.mountain_shade = Color{0x92, 0x52, 0x3e};
    t.hill_lit = Color{0xd4, 0x9c, 0x62};
    t.hill_shade = Color{0xb8, 0x84, 0x52};
    t.cloud_tint = Color{0xff, 0xf0, 0xd8};
    t.cloud_tint_amount = 0.3f;
    t.sun_amount = 0.6f;
    t.fog_density = 3.f;
    t.haze = 0.2f;
    t.mountain_scale = 0.6f;
    t.snow_line = 1.0e9f;
    t.lanes = 2;
    t.us_markings = true;
    z.theme.showers = 0.25f; // desert thunderstorms
    z.theme.night_glow = 0.02f; // the desert
    return z;
}

Zone zone_california() {
    Zone z{"USA", "CALIFORNIA", RoadTheme{}, 0, Decor::Coast};
    RoadTheme& t = z.theme;
    t.sky_top = Color{0xa8, 0xb8, 0xc4};
    t.sky_horizon = Color{0xd0, 0xd8, 0xdc};
    t.fog = Color{0xd0, 0xd8, 0xdc};
    t.grass[0] = Color{0x98, 0x90, 0x4c};
    t.grass[1] = Color{0x8c, 0x84, 0x44};
    t.road[0] = Color{0x66, 0x66, 0x68};
    t.road[1] = Color{0x5e, 0x5e, 0x60};
    t.mountain_lit = Color{0x8c, 0x98, 0xa0};
    t.mountain_shade = Color{0x74, 0x80, 0x8c};
    t.hill_lit = Color{0x88, 0x88, 0x58};
    t.hill_shade = Color{0x70, 0x70, 0x4c};
    t.rock[0] = Color{0x6c, 0x50, 0x38};
    t.rock[1] = Color{0x9c, 0x78, 0x50};
    t.rock[2] = Color{0xc0, 0x98, 0x68};
    t.cap = Color{0x7c, 0x8c, 0x48}; // scrub on top of the sandstone
    t.cap_amount = 0.5f;
    t.beyond[0] = Color{0x48, 0x70, 0x98};
    t.beyond[1] = Color{0x50, 0x7a, 0xa4};
    t.cloud_tint = Color{0xd0, 0xd8, 0xdc};
    t.cloud_tint_amount = 0.5f;
    t.fog_density = 18.f;
    t.haze = 0.8f;
    t.mountain_scale = 0.6f;
    t.snow_line = 1.0e9f;
    t.grip = 0.95f;
    t.lanes = 2;
    t.us_markings = true;
    z.theme.showers = 0.20f;
    return z;
}

Zone zone_san_francisco() {
    Zone z{"USA", "SAN FRANCISCO", RoadTheme{}, 0, Decor::City};
    RoadTheme& t = z.theme;
    t.sky_top = Color{0x6c, 0x9c, 0xd4};
    t.sky_horizon = Color{0xd8, 0xe4, 0xec};
    t.fog = Color{0xd4, 0xdc, 0xe4};
    t.grass[0] = Color{0xb8, 0xb4, 0xac}; // sidewalks
    t.grass[1] = Color{0xae, 0xaa, 0xa2};
    t.road[0] = Color{0x52, 0x52, 0x58};
    t.road[1] = Color{0x4c, 0x4c, 0x52};
    t.rumble[0] = Color{0xe8, 0xe8, 0xe0}; // kerbs
    t.rumble[1] = Color{0x9c, 0x9c, 0x98};
    t.mountain_lit = Color{0x8c, 0x9c, 0xb4};
    t.mountain_shade = Color{0x70, 0x80, 0x9c};
    t.hill_lit = Color{0x84, 0x94, 0x7c};
    t.hill_shade = Color{0x6c, 0x7c, 0x68};
    t.cloud_tint = Color{0xf0, 0xf4, 0xf8};
    t.cloud_tint_amount = 0.3f;
    t.sun_amount = 0.4f;
    t.fog_density = 6.f;
    t.haze = 0.45f;
    t.mountain_scale = 0.5f;
    t.hill_scale = 0.8f;
    t.snow_line = 1.0e9f;
    t.lanes = 2;
    t.us_markings = true;
    z.theme.showers = 0.35f;
    z.theme.night_glow = 0.7f; // the city
    return z;
}

// Plants scenery along the road according to each zone's decor rules. Sides
// that carry a rail or cliff stay free: nothing grows out of the rock or out
// of the sea.
void decorate(Track& track, TrackBuilder& b, int from, int to, uint32_t seed) {
    Rng rng(seed);
    int last_mesa = -1000;

    for (int i = std::max(from, 10); i < to; ++i) {
        const Segment& seg = track.segments[static_cast<size_t>(i)];
        if (seg.tunnel || track.segment(i + 3).tunnel) continue; // nothing grows inside, nor in the portal's way
        const Zone& zone = track.zones[static_cast<size_t>(track.zone_index[static_cast<size_t>(i)])];
        // Nothing grows on a forecourt, nor just before or after one.
        const bool forecourt = track.segment(i - 8).forecourt > 0.f || seg.forecourt > 0.f ||
                               track.segment(i + 8).forecourt > 0.f;
        const auto free_side = [&](int side) {
            const int court = track.look(i).left_hand ? -1 : 1;
            return (side < 0 ? seg.left : seg.right) == Edge::None && !(side == court && forecourt) &&
                   !(side == seg.facing_branch && seg.branch_bend);
        };
        const auto put = [&](Scenery kind, int side, float magnitude) {
            // Facing the other route of a fork, only between the two roads.
            if (side == seg.facing_branch && magnitude + scenery_info(kind).width / track.half_width(i) > 2.6f) return;
            if (free_side(side)) b.scenery(i, kind, static_cast<float>(side) * magnitude);
        };
        const auto both = [&](Scenery kind, float lo, float hi) {
            const float l = rng.range(lo, hi), r = rng.range(lo, hi);
            put(kind, -1, l);
            put(kind, +1, r);
        };
        const auto random_side = [&] { return rng.chance(0.5f) ? -1 : 1; };

        switch (zone.decor) {
            case Decor::Riviera:
                if (i % 5 == 0) both(Scenery::Palm, 1.15f, 1.35f);
                if (rng.chance(0.04f)) put(Scenery::Bush, random_side(), 2.2f);
                break;

            case Decor::Forest:
                for (int side = -1; side <= 1; side += 2) {
                    if (rng.chance(0.62f)) put(Scenery::Fir, side, rng.range(1.15f, 3.8f));
                }
                if (rng.chance(0.06f)) put(Scenery::Bush, random_side(), rng.range(1.1f, 2.4f));
                if (rng.chance(0.02f)) put(Scenery::Boulder, random_side(), rng.range(1.2f, 2.5f));
                break;

            case Decor::Alpine:
                for (int side = -1; side <= 1; side += 2) {
                    if (rng.chance(0.3f)) put(Scenery::SnowFir, side, rng.range(1.15f, 4.f));
                }
                if (rng.chance(0.014f)) put(Scenery::Chalet, random_side(), rng.range(1.5f, 2.8f));
                if (rng.chance(0.025f)) put(Scenery::Boulder, random_side(), rng.range(1.2f, 2.6f));
                break;

            case Decor::Tuscany: {
                // Cypress avenues alternate with open countryside.
                const bool avenue = (i / 90) % 3 == 0;
                if (avenue) {
                    if (i % 2 == 0) both(Scenery::Cypress, 1.12f, 1.22f);
                } else {
                    if (rng.chance(0.12f)) put(Scenery::Tree, random_side(), rng.range(1.3f, 4.f));
                    if (rng.chance(0.05f)) put(Scenery::Cypress, random_side(), rng.range(1.2f, 3.f));
                    if (rng.chance(0.06f)) put(Scenery::Bush, random_side(), rng.range(1.1f, 2.5f));
                    if (rng.chance(0.015f)) put(Scenery::Boulder, random_side(), rng.range(1.3f, 3.f));
                }
                break;
            }

            case Decor::Desert:
                if (i % 5 == 0) put(Scenery::Pole, +1, 1.2f);
                if (rng.chance(0.07f)) put(Scenery::Cactus, random_side(), rng.range(1.2f, 4.f));
                if (rng.chance(0.08f)) put(Scenery::DryShrub, random_side(), rng.range(1.1f, 3.f));
                if (rng.chance(0.03f)) put(Scenery::RedRock, random_side(), rng.range(1.3f, 3.5f));
                if (i - last_mesa > 70 && rng.chance(0.03f)) {
                    // Far from the road so the car can never touch them.
                    put(Scenery::Mesa, random_side(), rng.range(3.6f, 7.f));
                    last_mesa = i;
                }
                if (i % 220 == 110) put(Scenery::BillboardUs, i % 440 == 110 ? -1 : 1, 1.3f);
                break;

            case Decor::City: {
                // Row houses along both sidewalks, in random colours, with the
                // odd gap for a tree; street lamps at the kerb.
                constexpr Scenery houses[] = {Scenery::Victorian, Scenery::VictorianB, Scenery::VictorianC};
                for (int side = -1; side <= 1; side += 2) {
                    if ((i + (side > 0 ? 2 : 0)) % 4 != 0) continue;
                    if (rng.chance(0.12f)) put(Scenery::Tree, side, 1.3f);
                    else put(houses[static_cast<int>(rng.next() * 3.f) % 3], side, 1.35f);
                }
                if (i % 10 == 5) put(Scenery::StreetLamp, i % 20 == 5 ? -1 : 1, 1.12f);
                break;
            }

            case Decor::Town: {
                // Town houses, shops and blocks of flats along both
                // sidewalks, now and then one of the country's trees; street
                // lamps at the kerb.
                using S = Scenery;
                static constexpr Scenery styles[][6] = {
                    {S::Townhouse, S::TownhouseB, S::Shop, S::Apartment, S::Townhouse, S::TownhouseB}, // European
                    {S::Tower, S::Apartment, S::Shop, S::Tower, S::Apartment, S::Tower},                // Modern
                    {S::FlatHouse, S::FlatHouse, S::Shop, S::Apartment, S::FlatHouse, S::Shop},         // Warm
                };
                const Scenery* buildings = styles[static_cast<int>(zone.town)];
                for (int side = -1; side <= 1; side += 2) {
                    if ((i + (side > 0 ? 2 : 0)) % 4 != 0) continue;
                    if (rng.chance(0.14f)) put(zone.town_tree, side, 1.3f);
                    else put(buildings[static_cast<int>(rng.next() * 6.f) % 6], side, 1.35f);
                }
                if (i % 10 == 5) put(Scenery::StreetLamp, i % 20 == 5 ? -1 : 1, 1.12f);
                break;
            }

            case Decor::Country:
                // Hedgerows close along both sides, with stretches of stone
                // wall; oaks behind them and now and then a phone box.
                for (int side = -1; side <= 1; side += 2) {
                    if (i % 2 == (side > 0)) put((i / 60) % 3 == 1 ? Scenery::StoneWall : Scenery::Hedge, side, 1.12f);
                }
                if (rng.chance(0.1f)) put(Scenery::Tree, random_side(), rng.range(1.9f, 4.f));
                if (rng.chance(0.006f)) put(Scenery::PhoneBox, random_side(), 1.12f);
                break;

            case Decor::Polder:
                if (i % 3 == 0) both(Scenery::Tulips, 1.25f, 2.6f);
                if (rng.chance(0.03f)) put(Scenery::Windmill, random_side(), rng.range(2.4f, 5.f));
                if (rng.chance(0.03f)) put(Scenery::Tree, random_side(), rng.range(1.3f, 2.f));
                break;

            case Decor::Nile:
                if (i % 6 == 0) put(Scenery::DatePalm, +1, rng.range(1.2f, 1.6f));
                if (rng.chance(0.06f)) put(Scenery::DatePalm, -1, rng.range(1.2f, 3.f));
                if (i - last_mesa > 120 && rng.chance(0.02f)) {
                    put(Scenery::Pyramid, -1, rng.range(5.f, 8.f));
                    last_mesa = i;
                }
                if (rng.chance(0.04f)) put(Scenery::DryShrub, random_side(), rng.range(1.2f, 3.f));
                break;

            case Decor::Savanna:
                if (rng.chance(0.05f)) put(Scenery::Acacia, random_side(), rng.range(1.4f, 5.f));
                if (rng.chance(0.012f)) put(Scenery::Giraffe, random_side(), rng.range(1.6f, 3.5f));
                if (rng.chance(0.03f)) put(Scenery::TermiteMound, random_side(), rng.range(1.2f, 3.f));
                if (rng.chance(0.04f)) put(Scenery::DryShrub, random_side(), rng.range(1.1f, 3.f));
                break;

            case Decor::Rajasthan:
                if (rng.chance(0.05f)) put(Scenery::Banyan, random_side(), rng.range(1.3f, 3.5f));
                if (i % 7 == 0) put(Scenery::Palm, random_side(), rng.range(1.2f, 2.5f));
                if (i % 140 == 70) put(Scenery::Temple, i % 280 == 70 ? -1 : 1, 1.6f);
                if (rng.chance(0.012f)) put(Scenery::Cow, random_side(), 1.15f);
                break;

            case Decor::Autumn:
                if (i % 3 == 0) both(Scenery::Maple, 1.15f, 2.8f);
                if (i % 110 == 55) put(Scenery::HanokGate, i % 220 == 55 ? -1 : 1, 1.3f);
                if (rng.chance(0.03f)) put(Scenery::Fir, random_side(), rng.range(1.5f, 3.5f));
                break;

            case Decor::Outback:
                if (rng.chance(0.04f)) put(Scenery::GumTree, random_side(), rng.range(1.3f, 4.f));
                if (rng.chance(0.04f)) put(Scenery::TermiteMound, random_side(), rng.range(1.2f, 3.f));
                if (rng.chance(0.05f)) put(Scenery::DryShrub, random_side(), rng.range(1.1f, 3.f));
                if (i % 150 == 30) put(Scenery::KangarooSign, +1, 1.2f);
                if (i - last_mesa > 300 && rng.chance(0.02f)) {
                    put(Scenery::Uluru, random_side(), rng.range(6.f, 9.f));
                    last_mesa = i;
                }
                break;

            case Decor::Jungle:
                for (int side = -1; side <= 1; side += 2) {
                    if (rng.chance(0.55f)) put(Scenery::JungleTree, side, rng.range(1.15f, 3.5f));
                }
                if (rng.chance(0.2f)) put(Scenery::Banana, random_side(), rng.range(1.1f, 2.f));
                break;

            case Decor::Sakura:
                if (i % 4 == 0) both(Scenery::CherryTree, 1.15f, 2.4f);
                if (i % 90 == 45) put(Scenery::Torii, i % 180 == 45 ? -1 : 1, 1.3f);
                if (rng.chance(0.03f)) put(Scenery::StoneLantern, random_side(), 1.12f);
                break;

            case Decor::Coast:
                if (i % 6 == 0) put(Scenery::Pole, +1, 1.2f);
                if (rng.chance(0.06f)) put(Scenery::DryShrub, random_side(), rng.range(1.1f, 3.f));
                if (rng.chance(0.02f)) put(Scenery::RedRock, random_side(), rng.range(1.3f, 3.f));
                if (i % 300 == 150) put(Scenery::BillboardUs, -1, 1.3f);
                break;
        }
    }
}

// Patches on the road surface, each over a few segments, widest in the
// middle, clear of the start, the forecourts, the forks' bends and each
// other. Puddles where it rains or snows, in proportion; in fair weather
// hardly ever, in the desert never. Oil slicks rarely, anywhere.
void place_patches(Track& track, int from, int to, uint32_t seed) {
    Rng rng(seed);
    auto place = [&](Patch kind, auto chance) {
        int last_end = -1000;
        for (int i = std::max(from, 20); i < to - 12; ++i) {
            const Zone& zone = track.zones[static_cast<size_t>(track.zone_index[static_cast<size_t>(i)])];
            if (i - last_end < 20 || !rng.chance(chance(zone))) continue;
            const int length = static_cast<int>(kind == Patch::Oil ? rng.range(4.f, 8.f) : rng.range(5.f, 11.f));
            bool clear = true;
            for (int k = -2; k < length + 2; ++k) {
                const Segment& s = track.segment(i + k);
                clear = clear && s.forecourt == 0.f && !s.checker && !s.branch_bend && s.patch_w == 0.f &&
                        i + k < to;
            }
            if (!clear) continue;
            const float width = kind == Patch::Oil ? rng.range(0.12f, 0.28f) : rng.range(0.12f, 0.35f);
            const float centre = rng.range(-0.95f + width, 0.95f - width);
            for (int k = 0; k < length; ++k) {
                Segment& s = track.segments[static_cast<size_t>(i + k)];
                s.patch = kind;
                s.patch_x = centre;
                // Rounded rather than pointed: the root of a sine.
                s.patch_w = width * std::sqrt(std::sin(PI * (static_cast<float>(k) + 0.5f) / static_cast<float>(length)));
            }
            last_end = i + length;
            i += length;
        }
    };
    place(Patch::Water, [](const Zone& z) {
        if (z.decor == Decor::Desert) return 0.f;
        return 0.0003f + 0.03f * std::max(z.theme.rain, z.theme.snowfall);
    });
    place(Patch::Oil, [](const Zone&) { return 0.0006f; });
}

namespace {
Track finish_route(Track& track, TrackBuilder& b);
} // namespace

Track build_demo_track() {
    Track track;
    TrackBuilder b(track);

    // The route: six zones, one lap through five regions of Europe and the USA.
    // Section lengths are chosen so every zone is longer than its transition
    // and the height returns to zero at the end of the lap.

    b.begin_zone(zone_france());
    b.straight(Len::Short);
    b.low_rolling_hills();
    const int corniche = b.size(); // a cliff road above the sea
    b.curve(Len::Medium, Bend::Medium, Hill::Low);
    b.curve(Len::Medium, -Bend::Medium, Hill::Medium);
    b.road(Len::Medium, Len::Medium, Len::Medium, Bend::Easy, -Hill::Medium);
    b.mark(corniche, b.size(), Edge::Cliff, Edge::Rail);
    b.bumps();
    b.gas_station();

    b.begin_zone(zone_england());
    // Narrow lanes winding between hedgerows over the rolling hills.
    b.curve(Len::Short, Bend::Medium, Hill::Low);
    b.curve(Len::Short, -Bend::Hard, -Hill::Low / 2.f);
    b.low_rolling_hills();
    b.gas_station();
    b.curve(Len::Short, Bend::Hard, Hill::Low);
    b.curve(Len::Short, -Bend::Medium, -Hill::Low);
    b.car_wash(); // after the muddy lanes
    b.car_dealer();

    b.begin_zone(zone_netherlands());
    // Dead straight and flat, along a canal for a while.
    b.straight(Len::Medium);
    b.level_crossing();
    const int canal = b.size();
    b.curve(Len::Medium, Bend::Easy, Hill::None);
    b.straight(Len::Short);
    b.mark(canal, b.size(), Edge::Rail, Edge::None);
    b.gas_station();
    b.curve(Len::Medium, -Bend::Easy, Hill::None);
    b.hospital();

    b.begin_zone(zone_germany());
    b.curve(Len::Medium, -Bend::Medium, Hill::Low);
    b.bridge(Len::Short); // over the Rhine
    b.hill(Len::Medium, Hill::Medium);
    b.curve(Len::Medium, Bend::Hard, -Hill::Low);
    b.bumps();
    b.curve(Len::Medium, -Bend::Medium, -Hill::Medium);
    b.straight(Len::Medium);
    b.gas_station();
    b.level_crossing();
    b.chemical_plant(); // where the Rhine meets industry
    b.truckstop(); // before the Autobahn
    // The fast way or the scenic one.
    b.fork("AUTOBAHN", "LANDSTRASSE",
           [&] {
               b.straight(Len::Short);
               b.overpass();
               b.straight(Len::Short / 2);
               b.curve(Len::Long, -Bend::Easy, Hill::Low);
               b.curve(Len::Long, Bend::Easy, -Hill::Low);
           },
           [&] {
               b.curve(Len::Short, Bend::Hard, Hill::Low);
               b.curve(Len::Short, -Bend::Hard, Hill::Medium);
               b.bumps();
               b.curve(Len::Short, Bend::Medium, -Hill::Medium);
               b.curve(Len::Short, -Bend::Hard, -Hill::Low);
           });

    b.begin_zone(zone_switzerland());
    b.hill(Len::Medium, Hill::High);
    const int pass = b.size(); // the pass: a wall on one side, a drop on the other
    b.curve(Len::Medium, Bend::Hard, Hill::Medium);
    b.curve(Len::Medium, -Bend::Hard, Hill::Medium);
    b.mark(pass, b.size(), Edge::Cliff, Edge::Rail);
    b.curve(Len::Short, Bend::Medium, Hill::None);
    b.gas_station(); // up on the pass
    b.tunnel(Len::Short, -Bend::Easy, Hill::None); // through the top of the pass
    const int descent = b.size();
    b.curve(Len::Medium, -Bend::Medium, -Hill::High);
    b.hill(Len::Medium, -Hill::High);
    b.mark(descent, b.size(), Edge::Rail, Edge::Cliff);
    b.curve(Len::Medium, Bend::Hard, -Hill::Medium);

    b.begin_zone(zone_italy());
    b.low_rolling_hills();
    b.gas_station();
    b.curve(Len::Medium, Bend::Medium, Hill::Low);
    b.sports_dealer(); // Italian sports cars
    b.curve(Len::Medium, -Bend::Medium, -Hill::Low);
    b.curve(Len::Long, Bend::Easy, -Hill::Low);

    b.begin_zone(zone_egypt());
    // Along the Nile, past the pyramids.
    b.straight(Len::Medium);
    b.gas_station();
    b.curve(Len::Long, -Bend::Easy, Hill::None);

    b.begin_zone(zone_kenya());
    // Across the savanna towards the mountain.
    b.hill(Len::Medium, Hill::Low);
    b.curve(Len::Medium, Bend::Medium, Hill::None);
    b.gas_station();
    b.hill(Len::Medium, -Hill::Low);
    b.car_wash(); // the red dust of the savanna

    b.begin_zone(zone_india());
    b.curve(Len::Short, -Bend::Medium, Hill::None);
    b.curve(Len::Short, Bend::Medium, Hill::Low / 2.f);
    b.gas_station();
    b.curve(Len::Medium, -Bend::Easy, -Hill::Low / 2.f);
    b.level_crossing();
    b.motel();

    b.begin_zone(zone_korea());
    // Up through the autumn forest and down again.
    b.curve(Len::Medium, Bend::Hard, Hill::Medium);
    b.tunnel(Len::Short, Bend::Easy, Hill::None);
    b.gas_station();
    b.curve(Len::Medium, -Bend::Hard, -Hill::Medium);
    b.chemical_plant();
    b.hospital();
    b.car_dealer();

    b.begin_zone(zone_japan());
    // Past the cherry trees towards the mountain.
    b.curve(Len::Medium, Bend::Medium, Hill::Low);
    b.gas_station();
    b.curve(Len::Medium, -Bend::Medium, -Hill::Low);
    b.sports_dealer();

    b.begin_zone(zone_australia());
    // Dead straight through the red outback.
    b.straight(Len::Long);
    b.level_crossing();
    b.truckstop(); // where the road trains stop
    b.gas_station();
    b.curve(Len::Long, Bend::Easy, Hill::None);
    b.car_wash();

    b.begin_zone(zone_arizona());
    b.hill(Len::Long, Hill::Low);
    b.gas_station();
    b.level_crossing(); // a freight line across the desert
    b.chemical_plant();
    b.curve(Len::Long, Bend::Easy, Hill::None);
    // The old highway across the open desert, or through the canyon.
    b.fork("ROUTE 66", "CANYON ROAD",
           [&] {
               b.straight(Len::Long);
               b.hill(Len::Medium, -Hill::Low);
               b.bumps();
           },
           [&] {
               b.straight(Len::Short);
               const int canyon = b.size();
               b.curve(Len::Medium, Bend::Medium, Hill::None);
               b.curve(Len::Medium, -Bend::Hard, -Hill::Low);
               b.curve(Len::Short, Bend::Medium, Hill::None);
               b.mark(canyon, b.size(), Edge::Cliff, Edge::Cliff);
           });
    b.curve(Len::Medium, -Bend::Medium, Hill::Low);
    b.straight(Len::Medium);
    b.motel(); // on the old highway

    b.begin_zone(zone_california());
    const int pch = b.size(); // the coast road: the ocean on the left, sandstone on the right
    b.curve(Len::Medium, Bend::Medium, Hill::None);
    b.curve(Len::Medium, -Bend::Medium, Hill::Low);
    b.curve(Len::Medium, Bend::Hard, -Hill::Low);
    b.mark(pch, b.size(), Edge::Rail, Edge::Cliff);
    b.curve(Len::Medium, -Bend::Easy, Hill::None);
    b.gas_station();
    b.straight(Len::Short);
    b.sports_dealer(); // on the coast road

    b.begin_zone(zone_san_francisco());
    // Up from the waterfront, block after block, each street steeper than the
    // last and flat at every crossing; down into a valley and over the next
    // hill. Every crest taken fast is a jump.
    b.slope(12, 0.f);
    b.slope(30, 0.35f);
    b.slope(6, 0.f);
    b.slope(30, 0.5f);
    b.slope(6, 0.f);
    b.slope(30, -0.5f);
    b.slope(6, 0.f);
    b.slope(36, -0.55f, Bend::Easy);
    b.slope(10, 0.f); // the valley floor
    b.slope(30, 0.55f);
    b.slope(6, 0.f);
    b.slope(26, 0.45f, -Bend::Easy);
    b.slope(6, 0.f);
    b.slope(30, -0.45f);
    b.slope(6, 0.f);
    b.slope(30, -0.45f);
    b.slope(8, 0.f);
    b.gas_station();
    b.slope(30, 0.5f);
    b.slope(6, 0.f);
    b.slope(30, -0.5f, Bend::Medium);
    b.slope(8, 0.f);

    b.begin_zone(zone_brazil());
    // Through the rainforest in a downpour, and down to the start.
    b.bridge(Len::Short); // over the Amazon
    b.curve(Len::Medium, Bend::Medium, Hill::Low);
    b.gas_station();
    b.curve(Len::Medium, -Bend::Hard, Hill::None);
    b.downhill_to_end(Len::Long);

    return finish_route(track, b);
}

namespace {

// The common end of building a track: themes blended, the start line, the
// scenery and patches along the lap and each fork's other route.
Track finish_route(Track& track, TrackBuilder& b) {
    track.finish();

    // Start/finish line a few segments ahead of the starting grid.
    const int start = 8;
    track.start_z = static_cast<float>(start) * track.segment_length;
    track.segments[start].checker = true;
    track.segments[start + 1].checker = true;
    b.scenery(start, Scenery::Gantry, 0.f);

    // Scenery and patches along the whole lap, the right routes of the forks
    // being in the track now; then along each left route.
    const int n = static_cast<int>(track.segments.size());
    decorate(track, b, 0, n, 0x6b75727au);
    place_patches(track, 0, n, 0x77657473u);
    for (size_t i = 0; i < track.branches.size(); ++i) {
        const Branch& br = track.branches[i];
        track.choose_branch(i, 0);
        decorate(track, b, br.fork, br.end(), 0x6c656674u + static_cast<uint32_t>(i));
        place_patches(track, br.fork, br.end(), 0x6c657774u + static_cast<uint32_t>(i));
        track.choose_branch(i, 1);
    }

    // Billboards greeting the driver along the start straight.
    for (int i = 20; i < 160; i += 20) {
        b.scenery(i, Scenery::Billboard, (i / 20) % 2 ? -1.15f : 1.15f);
    }
    return track;
}

// A city of the grand tour in the look of `country`: sidewalks for verges,
// kerbs, the country's lanes and sky; town houses, shops and flats.
Zone city(Zone country, const char* name, Scenery tree, TownStyle style = TownStyle::European) {
    Zone z = std::move(country);
    z.region = name;
    z.decor = Decor::Town;
    z.town_tree = tree;
    z.town = style;
    z.theme.night_glow = 0.75f; // street lights, windows, the sky aglow
    RoadTheme& t = z.theme;
    t.grass[0] = blend(Color{0xb8, 0xb4, 0xac}, t.grass[0], 0.15f);
    t.grass[1] = blend(Color{0xae, 0xaa, 0xa2}, t.grass[1], 0.15f);
    t.rumble[0] = Color{0xe8, 0xe8, 0xe0};
    t.rumble[1] = Color{0x9c, 0x9c, 0x98};
    t.road_scale = std::max(t.road_scale, 0.8f);
    t.haze = std::max(t.haze, 0.35f);
    return z;
}

Zone renamed(Zone z, const char* region) {
    z.region = region;
    return z;
}

} // namespace

const char* track_name(int index) {
    return index == 1 ? "GRAND TOUR" : "SMALL WORLD";
}

Track build_track(int index) {
    if (index != 1) return build_demo_track();
    Track track;
    TrackBuilder b(track);

    // Around the same world as the small one, but stopping in the cities:
    // through each country from one city to the next, the countryside in
    // between. City streets: short blocks, square turns, flat.
    const auto streets = [&](int turn) {
        const float s = static_cast<float>(turn);
        b.straight(Len::Short);
        b.curve(Len::Short, s * Bend::Hard, Hill::None);
        b.straight(Len::Short);
        b.curve(Len::Short, -s * Bend::Medium, Hill::Low / 4.f);
        b.straight(Len::Short);
        b.curve(Len::Short, s * Bend::Medium, -Hill::Low / 4.f);
    };

    b.begin_zone(city(zone_france(), "PARIS", Scenery::Tree));
    b.straight(Len::Short);
    b.overpass();
    streets(1);
    b.gas_station();
    streets(-1);
    Zone bourgogne = renamed(zone_italy(), "BOURGOGNE"); // vineyards and cypresses
    bourgogne.country = "FRANCE";
    b.begin_zone(bourgogne);
    b.low_rolling_hills();
    b.curve(Len::Medium, Bend::Easy, Hill::Low);
    b.curve(Len::Medium, -Bend::Medium, -Hill::Low);
    b.car_wash();
    b.begin_zone(city(zone_france(), "LYON", Scenery::Tree));
    streets(1);
    b.car_dealer();
    b.begin_zone(zone_france()); // the Corniche above the sea
    b.low_rolling_hills();
    const int corniche = b.size();
    b.curve(Len::Medium, Bend::Medium, Hill::Low);
    b.curve(Len::Medium, -Bend::Medium, Hill::Medium);
    b.road(Len::Medium, Len::Medium, Len::Medium, Bend::Easy, -Hill::Medium);
    b.mark(corniche, b.size(), Edge::Cliff, Edge::Rail);
    b.gas_station();
    b.curve(Len::Short, Bend::Easy, -Hill::Low);
    b.begin_zone(city(zone_france(), "NICE", Scenery::Palm));
    streets(-1);
    b.sports_dealer();

    b.begin_zone(city(zone_england(), "LONDON", Scenery::Tree));
    streets(1);
    b.gas_station();
    streets(1);
    b.hospital();
    b.begin_zone(zone_england());
    b.curve(Len::Short, Bend::Medium, Hill::Low);
    b.curve(Len::Short, -Bend::Hard, -Hill::Low / 2.f);
    b.low_rolling_hills();
    b.gas_station();
    b.curve(Len::Short, Bend::Hard, Hill::Low);
    b.curve(Len::Short, -Bend::Medium, -Hill::Low);
    b.car_wash();
    b.begin_zone(city(zone_england(), "OXFORD", Scenery::Tree));
    streets(-1);
    b.car_dealer();

    b.begin_zone(city(zone_netherlands(), "AMSTERDAM", Scenery::Tree));
    const int gracht = b.size(); // along a canal
    streets(1);
    b.mark(gracht, b.size(), Edge::Rail, Edge::None);
    b.gas_station();
    b.begin_zone(zone_netherlands());
    b.straight(Len::Medium);
    b.level_crossing();
    const int canal = b.size();
    b.curve(Len::Medium, Bend::Easy, Hill::None);
    b.straight(Len::Medium);
    b.mark(canal, b.size(), Edge::Rail, Edge::None);
    b.gas_station();
    b.curve(Len::Medium, -Bend::Easy, Hill::None);
    b.begin_zone(city(zone_netherlands(), "ROTTERDAM", Scenery::Tree));
    streets(-1);
    b.truckstop(); // by the port
    b.chemical_plant();

    b.begin_zone(city(zone_germany(), "KOELN", Scenery::Tree));
    streets(1);
    b.gas_station();
    b.begin_zone(zone_germany());
    b.curve(Len::Medium, -Bend::Medium, Hill::Low);
    b.bridge(Len::Short); // over the Rhine
    b.hill(Len::Medium, Hill::Medium);
    b.curve(Len::Medium, Bend::Hard, -Hill::Low);
    b.bumps();
    b.curve(Len::Medium, -Bend::Medium, -Hill::Medium);
    b.gas_station();
    b.level_crossing();
    b.chemical_plant();
    b.fork("AUTOBAHN", "LANDSTRASSE",
           [&] {
               b.straight(Len::Long);
               b.curve(Len::Long, -Bend::Easy, Hill::Low);
               b.curve(Len::Long, Bend::Easy, -Hill::Low);
           },
           [&] {
               b.curve(Len::Medium, Bend::Hard, Hill::Low);
               b.curve(Len::Medium, -Bend::Hard, Hill::Medium);
               b.bumps();
               b.curve(Len::Medium, Bend::Medium, -Hill::Medium);
               b.curve(Len::Medium, -Bend::Hard, -Hill::Low);
           });
    b.begin_zone(city(zone_germany(), "MUENCHEN", Scenery::Tree));
    streets(-1);
    b.sports_dealer();

    b.begin_zone(city(zone_switzerland(), "ZUERICH", Scenery::Fir));
    streets(1);
    b.gas_station();
    b.begin_zone(zone_switzerland());
    b.hill(Len::Medium, Hill::High);
    const int pass = b.size();
    b.curve(Len::Medium, Bend::Hard, Hill::Medium);
    b.curve(Len::Medium, -Bend::Hard, Hill::Medium);
    b.mark(pass, b.size(), Edge::Cliff, Edge::Rail);
    b.curve(Len::Short, Bend::Medium, Hill::None);
    b.gas_station();
    b.tunnel(Len::Medium, -Bend::Easy, Hill::None); // the long tunnel under the pass
    const int descent = b.size();
    b.curve(Len::Medium, -Bend::Medium, -Hill::High);
    b.hill(Len::Medium, -Hill::High);
    b.mark(descent, b.size(), Edge::Rail, Edge::Cliff);
    b.curve(Len::Medium, Bend::Hard, -Hill::Medium);
    b.begin_zone(city(zone_switzerland(), "GENEVE", Scenery::Fir));
    streets(-1);
    b.hospital();

    b.begin_zone(city(zone_italy(), "MILANO", Scenery::Cypress));
    streets(1);
    b.gas_station();
    b.begin_zone(zone_italy());
    b.low_rolling_hills();
    b.curve(Len::Medium, Bend::Medium, Hill::Low);
    b.gas_station();
    b.curve(Len::Medium, -Bend::Medium, -Hill::Low);
    b.curve(Len::Long, Bend::Easy, Hill::None);
    b.begin_zone(city(zone_italy(), "ROMA", Scenery::Cypress));
    streets(-1);
    b.sports_dealer();
    streets(1);

    b.begin_zone(city(zone_egypt(), "CAIRO", Scenery::DatePalm, TownStyle::Warm));
    streets(1);
    b.gas_station();
    b.begin_zone(zone_egypt());
    b.straight(Len::Medium);
    b.gas_station();
    b.curve(Len::Long, -Bend::Easy, Hill::None);
    b.straight(Len::Long);
    b.begin_zone(city(zone_egypt(), "LUXOR", Scenery::DatePalm, TownStyle::Warm));
    streets(-1);
    b.motel();

    b.begin_zone(city(zone_kenya(), "NAIROBI", Scenery::Acacia, TownStyle::Warm));
    streets(1);
    b.gas_station();
    b.begin_zone(zone_kenya());
    b.hill(Len::Medium, Hill::Low);
    b.curve(Len::Medium, Bend::Medium, Hill::None);
    b.gas_station();
    b.hill(Len::Medium, -Hill::Low);
    b.curve(Len::Long, -Bend::Easy, Hill::None);
    b.car_wash();

    b.begin_zone(city(zone_india(), "MUMBAI", Scenery::Banyan, TownStyle::Warm));
    streets(1);
    b.gas_station();
    streets(-1);
    b.begin_zone(zone_india());
    b.curve(Len::Short, -Bend::Medium, Hill::None);
    b.curve(Len::Short, Bend::Medium, Hill::Low / 2.f);
    b.gas_station();
    b.curve(Len::Medium, -Bend::Easy, -Hill::Low / 2.f);
    b.level_crossing();
    b.motel();
    b.begin_zone(city(zone_india(), "DELHI", Scenery::Banyan, TownStyle::Warm));
    streets(1);
    b.hospital();

    b.begin_zone(city(zone_korea(), "SEOUL", Scenery::Maple, TownStyle::Modern));
    streets(-1);
    b.gas_station();
    b.begin_zone(zone_korea());
    b.curve(Len::Medium, Bend::Hard, Hill::Medium);
    b.gas_station();
    b.curve(Len::Medium, -Bend::Hard, -Hill::Medium);
    b.begin_zone(city(zone_korea(), "BUSAN", Scenery::Maple, TownStyle::Modern));
    streets(1);
    b.car_dealer();

    b.begin_zone(city(zone_japan(), "TOKYO", Scenery::CherryTree, TownStyle::Modern));
    streets(1);
    b.gas_station();
    streets(-1);
    b.sports_dealer();
    b.begin_zone(zone_japan());
    b.curve(Len::Medium, Bend::Medium, Hill::Low);
    b.tunnel(Len::Short, Bend::Easy, Hill::None);
    b.gas_station();
    b.curve(Len::Medium, -Bend::Medium, -Hill::Low);
    b.begin_zone(city(zone_japan(), "KYOTO", Scenery::CherryTree, TownStyle::Modern));
    streets(-1);
    b.motel();

    b.begin_zone(city(zone_australia(), "SYDNEY", Scenery::GumTree, TownStyle::Modern));
    streets(1);
    b.gas_station();
    b.begin_zone(zone_australia());
    b.straight(Len::Long);
    b.level_crossing();
    b.truckstop();
    b.gas_station();
    b.curve(Len::Long, Bend::Easy, Hill::None);
    b.straight(Len::Long);
    b.gas_station();
    b.car_wash();
    b.begin_zone(city(zone_australia(), "MELBOURNE", Scenery::GumTree, TownStyle::Modern));
    streets(-1);
    b.car_dealer();

    b.begin_zone(city(zone_arizona(), "PHOENIX", Scenery::Palm, TownStyle::Warm));
    streets(1);
    b.gas_station();
    b.begin_zone(zone_arizona());
    b.hill(Len::Long, Hill::Low);
    b.gas_station();
    b.level_crossing();
    b.chemical_plant();
    b.curve(Len::Long, Bend::Easy, Hill::None);
    b.fork("ROUTE 66", "CANYON ROAD",
           [&] {
               b.straight(Len::Long);
               b.hill(Len::Medium, -Hill::Low);
               b.bumps();
           },
           [&] {
               b.straight(Len::Short);
               const int canyon = b.size();
               b.curve(Len::Medium, Bend::Medium, Hill::None);
               b.curve(Len::Medium, -Bend::Hard, -Hill::Low);
               b.curve(Len::Short, Bend::Medium, Hill::None);
               b.mark(canyon, b.size(), Edge::Cliff, Edge::Cliff);
           });
    b.motel();
    b.begin_zone(city(zone_california(), "LOS ANGELES", Scenery::Palm, TownStyle::Modern));
    b.overpass(); // the freeway over the street
    streets(-1);
    b.gas_station();
    streets(1);
    b.sports_dealer();
    b.begin_zone(zone_california());
    const int pch = b.size();
    b.curve(Len::Medium, Bend::Medium, Hill::None);
    b.curve(Len::Medium, -Bend::Medium, Hill::Low);
    b.curve(Len::Medium, Bend::Hard, -Hill::Low);
    b.mark(pch, b.size(), Edge::Rail, Edge::Cliff);
    b.gas_station();
    b.begin_zone(zone_san_francisco());
    b.slope(12, 0.f);
    b.slope(30, 0.35f);
    b.slope(6, 0.f);
    b.slope(30, 0.5f);
    b.slope(6, 0.f);
    b.slope(30, -0.5f);
    b.slope(6, 0.f);
    b.slope(36, -0.35f, Bend::Easy);
    b.slope(8, 0.f);
    b.gas_station();

    b.begin_zone(city(zone_brazil(), "SAO PAULO", Scenery::Palm, TownStyle::Modern));
    streets(1);
    b.gas_station();
    b.begin_zone(zone_brazil());
    b.bridge(Len::Short);
    b.curve(Len::Medium, Bend::Medium, Hill::Low);
    b.gas_station();
    b.curve(Len::Medium, -Bend::Hard, Hill::None);
    b.begin_zone(city(zone_brazil(), "RIO DE JANEIRO", Scenery::Palm, TownStyle::Warm));
    streets(-1);
    b.hospital();
    b.downhill_to_end(Len::Long);

    return finish_route(track, b);
}

} // namespace racer
