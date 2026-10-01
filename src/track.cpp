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

Track build_demo_track() {
    Track track;
    TrackBuilder b(track);

    b.straight(Len::Short);
    b.low_rolling_hills();
    const int corniche = b.size();
    b.s_curves();
    b.mark(corniche, b.size(), Edge::Cliff, Edge::Rail);
    b.curve(Len::Medium, Bend::Medium, Hill::Low);
    b.bumps();
    b.low_rolling_hills();
    b.curve(Len::Long * 2, Bend::Medium, Hill::Medium);
    b.straight(Len::Medium);
    b.hill(Len::Medium, Hill::High);
    b.s_curves();
    b.curve(Len::Long, -Bend::Medium, Hill::None);
    b.hill(Len::Long, Hill::High);
    b.curve(Len::Long, Bend::Medium, -Hill::Low);
    b.bumps();
    b.hill(Len::Long, -Hill::Medium);
    b.straight(Len::Medium);
    b.curve(Len::Medium, -Bend::Hard, Hill::None);
    b.s_curves();
    b.downhill_to_end(200);

    track.zones.push_back(Zone{"FRANCE", "COTE D'AZUR", RoadTheme{}, 0});

    // Start/finish line a few segments ahead of the starting grid.
    const int start = 8;
    track.start_z = static_cast<float>(start) * track.segment_length;
    track.segments[start].checker = true;
    track.segments[start + 1].checker = true;
    b.scenery(start, Scenery::Gantry, 0.f);

    const int n = static_cast<int>(track.segments.size());
    Rng rng(0x6b75727au);

    // Billboards greeting the driver along the start straight.
    for (int i = 20; i < 160; i += 20) {
        b.scenery(i, Scenery::Billboard, (i / 20) % 2 ? -1.15f : 1.15f);
    }

    for (int i = 10; i < n; ++i) {
        const float progress = static_cast<float>(i) / static_cast<float>(n);

        if (progress < 0.18f || (progress > 0.62f && progress < 0.8f)) {
            // Beach sections: palm avenues on both sides.
            if (i % 5 == 0) {
                b.scenery(i, Scenery::Palm, -rng.range(1.15f, 1.35f));
                b.scenery(i, Scenery::Palm, rng.range(1.15f, 1.35f));
            }
            if (rng.chance(0.04f)) b.scenery(i, Scenery::Bush, rng.chance(0.5f) ? -2.2f : 2.2f);
        } else {
            // Countryside: scattered trees, bushes and rocks.
            if (rng.chance(0.18f)) {
                const float side = rng.chance(0.5f) ? -1.f : 1.f;
                b.scenery(i, Scenery::Tree, side * rng.range(1.2f, 4.f));
            }
            if (rng.chance(0.06f)) {
                const float side = rng.chance(0.5f) ? -1.f : 1.f;
                b.scenery(i, Scenery::Bush, side * rng.range(1.1f, 2.5f));
            }
            if (rng.chance(0.03f)) {
                const float side = rng.chance(0.5f) ? -1.f : 1.f;
                b.scenery(i, Scenery::Boulder, side * rng.range(1.2f, 3.f));
            }
        }

        if (i % 400 == 200) {
            b.scenery(i, Scenery::Billboard, (i / 400) % 2 ? -1.15f : 1.15f);
        }
    }

    track.finish();
    return track;
}

} // namespace racer
