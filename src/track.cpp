// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "track.hpp"

#include <algorithm>
#include <cassert>
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

float Track::forecourt_at(int boundary) const {
    return std::min(segment(boundary).forecourt, segment(boundary - 1).forecourt);
}

bool Track::on_forecourt(float z, float x) const {
    const float edge = segment_at(z).forecourt;
    return edge > 1.f && x > 1.f && x < edge;
}

std::vector<int> Track::gas_stations() const {
    std::vector<int> starts;
    const int n = static_cast<int>(segments.size());
    for (int i = 0; i < n; ++i) {
        if (segment(i).forecourt >= forecourt_width && segment(i - 1).forecourt < forecourt_width) starts.push_back(i);
    }
    return starts;
}

float Track::wet_width_at(int boundary) const {
    return std::min(segment(boundary).wet_w, segment(boundary - 1).wet_w);
}

float Track::wet_center_at(int boundary) const {
    return segment(boundary).wet_w > 0.f ? segment(boundary).wet_x : segment(boundary - 1).wet_x;
}

bool Track::on_wet(float z, float x, float half_width) const {
    const Segment& s = segment_at(z);
    return s.wet_w > 0.f && std::abs(x - s.wet_x) < s.wet_w + half_width;
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
    static_assert(sizeof(RoadTheme) == 176, "RoadTheme changed: update mix_themes()");

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
    r.haze = f(a.haze, b.haze);
    r.mountain_scale = f(a.mountain_scale, b.mountain_scale);
    r.hill_scale = f(a.hill_scale, b.hill_scale);
    r.snow_line = f(a.snow_line, b.snow_line);
    r.rain = f(a.rain, b.rain);
    r.snowfall = f(a.snowfall, b.snowfall);
    r.grip = f(a.grip, b.grip);
    r.center_line = c(a.center_line, b.center_line);
    // lanes and us_markings are discrete: they stay those of the nearer theme.
    return r;
}

void Track::finish(int transition_segments) {
    assert(!zones.empty() && zones.front().first_segment == 0);
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
namespace Len { constexpr int None = 0, Short = 25, Medium = 50, Long = 100; }
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
    void gas_station() {
        const int from = size();
        road(8, 40, 8, Bend::None, Hill::None);
        constexpr int start = 6, taper = 4, length = 44;
        for (int i = 0; i < length; ++i) {
            const float in = std::min(static_cast<float>(i + 1), static_cast<float>(length - i)) / taper;
            t_.segments[static_cast<size_t>(from + start + i)].forecourt =
                1.f + (forecourt_width - 1.f) * std::min(1.f, in);
        }
        scenery(from + 1, Scenery::FuelSign, 1.25f);
        scenery(from + start + 16, Scenery::FuelPump, 1.75f);
        scenery(from + start + 24, Scenery::FuelPump, 1.75f);
        scenery(from + start + 30, Scenery::GasStation, forecourt_width + 0.1f);
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
    return z;
}

// Plants scenery along the road according to each zone's decor rules. Sides
// that carry a rail or cliff stay free: nothing grows out of the rock or out
// of the sea.
void decorate(Track& track, TrackBuilder& b) {
    Rng rng(0x6b75727au);
    const int n = static_cast<int>(track.segments.size());
    int last_mesa = -1000;

    for (int i = 10; i < n; ++i) {
        const Segment& seg = track.segments[static_cast<size_t>(i)];
        const Zone& zone = track.zones[static_cast<size_t>(track.zone_index[static_cast<size_t>(i)])];
        // Nothing grows on a forecourt, nor just before or after one.
        const bool forecourt = track.segment(i - 8).forecourt > 0.f || seg.forecourt > 0.f ||
                               track.segment(i + 8).forecourt > 0.f;
        const auto free_side = [&](int side) {
            return (side < 0 ? seg.left : seg.right) == Edge::None && !(side > 0 && forecourt);
        };
        const auto put = [&](Scenery kind, int side, float magnitude) {
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

            case Decor::Coast:
                if (i % 6 == 0) put(Scenery::Pole, +1, 1.2f);
                if (rng.chance(0.06f)) put(Scenery::DryShrub, random_side(), rng.range(1.1f, 3.f));
                if (rng.chance(0.02f)) put(Scenery::RedRock, random_side(), rng.range(1.3f, 3.f));
                if (i % 300 == 150) put(Scenery::BillboardUs, -1, 1.3f);
                break;
        }
    }

    // Billboards greeting the driver along the start straight.
    for (int i = 20; i < 160; i += 20) {
        b.scenery(i, Scenery::Billboard, (i / 20) % 2 ? -1.15f : 1.15f);
    }
}

// Wet spots on the road: plenty where it rains or snows, the odd one in
// fair weather, none in the desert. Each runs over a few segments, widest in
// the middle, and keeps clear of the start and the forecourts.
void place_wet_spots(Track& track) {
    Rng rng(0x77657473u);
    const int n = static_cast<int>(track.segments.size());
    int last_end = -1000;
    for (int i = 20; i < n - 12; ++i) {
        const Zone& zone = track.zones[static_cast<size_t>(track.zone_index[static_cast<size_t>(i)])];
        if (zone.decor == Decor::Desert || i - last_end < 20) continue;
        const float wetness = std::max(zone.theme.rain, zone.theme.snowfall);
        if (!rng.chance(0.003f + 0.03f * wetness)) continue;
        const int length = static_cast<int>(rng.range(5.f, 11.f));
        bool clear = true;
        for (int k = -2; k < length + 2; ++k) {
            const Segment& s = track.segment(i + k);
            clear = clear && s.forecourt == 0.f && !s.checker;
        }
        if (!clear) continue;
        const float width = rng.range(0.12f, 0.35f);
        const float centre = rng.range(-0.95f + width, 0.95f - width);
        for (int k = 0; k < length; ++k) {
            Segment& s = track.segments[static_cast<size_t>(i + k)];
            s.wet_x = centre;
            s.wet_w = width * std::sin(PI * (static_cast<float>(k) + 0.5f) / static_cast<float>(length));
        }
        last_end = i + length;
        i += length;
    }
}

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

    b.begin_zone(zone_germany());
    b.curve(Len::Medium, -Bend::Medium, Hill::Low);
    b.hill(Len::Medium, Hill::Medium);
    b.curve(Len::Medium, Bend::Hard, -Hill::Low);
    b.bumps();
    b.curve(Len::Medium, -Bend::Medium, -Hill::Medium);
    b.straight(Len::Medium);
    b.gas_station();

    b.begin_zone(zone_switzerland());
    b.hill(Len::Medium, Hill::High);
    const int pass = b.size(); // the pass: a wall on one side, a drop on the other
    b.curve(Len::Medium, Bend::Hard, Hill::Medium);
    b.curve(Len::Medium, -Bend::Hard, Hill::Medium);
    b.mark(pass, b.size(), Edge::Cliff, Edge::Rail);
    b.curve(Len::Short, Bend::Medium, Hill::None);
    b.gas_station(); // up on the pass
    const int descent = b.size();
    b.curve(Len::Medium, -Bend::Medium, -Hill::High);
    b.hill(Len::Medium, -Hill::High);
    b.mark(descent, b.size(), Edge::Rail, Edge::Cliff);
    b.curve(Len::Medium, Bend::Hard, -Hill::Medium);

    b.begin_zone(zone_italy());
    b.low_rolling_hills();
    b.gas_station();
    b.curve(Len::Medium, Bend::Medium, Hill::Low);
    b.curve(Len::Medium, -Bend::Medium, -Hill::Low);
    b.curve(Len::Long, Bend::Easy, -Hill::Low);

    b.begin_zone(zone_arizona());
    b.hill(Len::Long, Hill::Low);
    b.gas_station();
    b.curve(Len::Long, Bend::Easy, Hill::None);
    b.hill(Len::Medium, -Hill::Low);
    b.bumps();
    b.curve(Len::Medium, -Bend::Medium, Hill::Low);
    b.straight(Len::Medium);

    b.begin_zone(zone_california());
    const int pch = b.size(); // the coast road: the ocean on the left, sandstone on the right
    b.curve(Len::Medium, Bend::Medium, Hill::None);
    b.curve(Len::Medium, -Bend::Medium, Hill::Low);
    b.curve(Len::Medium, Bend::Hard, -Hill::Low);
    b.mark(pch, b.size(), Edge::Rail, Edge::Cliff);
    b.curve(Len::Medium, -Bend::Easy, Hill::None);
    b.gas_station();
    b.downhill_to_end(Len::Long);

    track.finish();

    // Start/finish line a few segments ahead of the starting grid.
    const int start = 8;
    track.start_z = static_cast<float>(start) * track.segment_length;
    track.segments[start].checker = true;
    track.segments[start + 1].checker = true;
    b.scenery(start, Scenery::Gantry, 0.f);

    decorate(track, b);
    place_wet_spots(track);
    return track;
}

} // namespace racer
