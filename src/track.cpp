#include "track.hpp"

#include <cmath>
#include <cstdint>

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
    };
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
    b.s_curves();
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

    return track;
}

} // namespace racer
