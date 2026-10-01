// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

// Minimal self-contained unit tests; no framework needed. Run with `ctest`
// or directly: ./kurvenrausch_tests

#include "input.hpp"
#include "track.hpp"
#include "weather.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <cstdio>
#include <string>

namespace {

int failures = 0;

#define CHECK(cond)                                                                \
    do {                                                                           \
        if (!(cond)) {                                                             \
            std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
            ++failures;                                                            \
        }                                                                          \
    } while (0)

#define CHECK_NEAR(a, b, eps) CHECK(std::abs((a) - (b)) <= (eps))

void test_deadzone() {
    using racer::apply_deadzone;
    CHECK_NEAR(apply_deadzone(0.f, 0.2f), 0.f, 1e-6f);
    CHECK_NEAR(apply_deadzone(0.1f, 0.2f), 0.f, 1e-6f);
    CHECK_NEAR(apply_deadzone(-0.2f, 0.2f), 0.f, 1e-6f);
    CHECK_NEAR(apply_deadzone(1.f, 0.2f), 1.f, 1e-6f);
    CHECK_NEAR(apply_deadzone(-1.f, 0.2f), -1.f, 1e-6f);
    // Continuous at the edge of the dead zone, linear beyond it.
    CHECK_NEAR(apply_deadzone(0.2001f, 0.2f), 0.f, 1e-3f);
    CHECK_NEAR(apply_deadzone(0.6f, 0.2f), 0.5f, 1e-6f);
    CHECK_NEAR(apply_deadzone(-0.6f, 0.2f), -0.5f, 1e-6f);
}

void test_pad_mapping() {
    using racer::InputState;
    using racer::PadState;

    {   // Idle pad changes nothing.
        InputState in;
        racer::merge_pad(in, PadState{});
        CHECK(in.throttle == 0.f && in.brake == 0.f && in.steer == 0.f);
    }
    {   // Analog stick and triggers.
        InputState in;
        PadState pad;
        pad.left_x = -1.f;
        pad.trigger_right = 1.f;
        pad.trigger_left = 0.5f;
        racer::merge_pad(in, pad);
        CHECK_NEAR(in.steer, -1.f, 1e-6f);
        CHECK_NEAR(in.throttle, 1.f, 1e-6f);
        CHECK(in.brake > 0.4f && in.brake < 0.5f);
    }
    {   // Stick drift inside the dead zone is ignored.
        InputState in;
        PadState pad;
        pad.left_x = 0.1f;
        pad.trigger_right = 0.03f;
        racer::merge_pad(in, pad);
        CHECK(in.steer == 0.f && in.throttle == 0.f);
    }
    {   // D-pad and face buttons are digital alternatives.
        InputState in;
        PadState pad;
        pad.dpad_right = true;
        pad.a = true;
        pad.b = true;
        racer::merge_pad(in, pad);
        CHECK_NEAR(in.steer, 1.f, 1e-6f);
        CHECK_NEAR(in.throttle, 1.f, 1e-6f);
        CHECK_NEAR(in.brake, 1.f, 1e-6f);
    }
    {   // Keyboard and pad combine: steering is clamped, pedals take the maximum.
        InputState in;
        in.steer = 1.f;
        in.throttle = 0.3f;
        PadState pad;
        pad.dpad_right = true;
        pad.trigger_right = 0.8f;
        racer::merge_pad(in, pad);
        CHECK_NEAR(in.steer, 1.f, 1e-6f);
        CHECK_NEAR(in.throttle, 0.8f, 0.05f);
        // Opposite inputs cancel.
        in.steer = 1.f;
        PadState left;
        left.dpad_left = true;
        racer::merge_pad(in, left);
        CHECK_NEAR(in.steer, 0.f, 1e-6f);
    }
}

// A flat 300 segment track with three zones starting at 0, 100 and 200.
racer::Track make_zoned_track() {
    using namespace racer;
    Track t;
    t.segments.resize(300);
    const Color sky[3] = {{10, 20, 30}, {110, 120, 130}, {210, 220, 230}};
    const float fog[3] = {2.f, 12.f, 22.f};
    for (int i = 0; i < 3; ++i) {
        Zone z;
        z.country = "C" + std::to_string(i);
        z.theme.sky_top = sky[i];
        z.theme.fog_density = fog[i];
        z.first_segment = i * 100;
        t.zones.push_back(z);
    }
    t.finish(40);
    return t;
}

void test_zones() {
    using namespace racer;
    const Track t = make_zoned_track();

    // Away from the boundaries (100, 200 and the seam at 300 == 0) a segment
    // has exactly its zone's look.
    CHECK(t.look(50).sky_top.r == 10);
    CHECK(t.look(150).sky_top.r == 110);
    CHECK(t.look(250).sky_top.r == 210);
    CHECK_NEAR(t.look(150).fog_density, 12.f, 1e-6f);

    // Zone lookup by position follows the zone ranges, not the blend.
    CHECK(t.zone_number_at(99.f * 200.f) == 0);
    CHECK(t.zone_number_at(100.f * 200.f) == 1);
    CHECK(t.zone_number_at(299.f * 200.f) == 2);
    CHECK(t.zone_at(150.f * 200.f).country == "C1");
    CHECK(t.zone_number_at(300.f * 200.f) == 0); // wraps around the lap

    // Across a boundary the look changes monotonically and ends up in the
    // next zone; the middle sits about half way.
    int previous = t.look(80).sky_top.r;
    for (int i = 81; i < 120; ++i) {
        const int r = t.look(i).sky_top.r;
        CHECK(r >= previous);
        previous = r;
    }
    CHECK(t.look(79).sky_top.r == 10);
    CHECK(t.look(120).sky_top.r == 110);
    CHECK(std::abs(t.look(100).sky_top.r - 60) <= 6);

    // The lap seam blends the last zone into the first one without a jump.
    // The fade is centred on the seam: segments 280..299 and 0..19.
    CHECK(t.look(279).sky_top.r == 210);
    CHECK(t.look(280).sky_top.r >= 208);
    CHECK(std::abs(t.look(299).sky_top.r - 110) < 15); // about half way
    CHECK(std::abs(t.look(0).sky_top.r - 110) < 15);
    CHECK(std::abs(t.look(299).sky_top.r - t.look(0).sky_top.r) < 15);
    CHECK(t.look(19).sky_top.r <= 12);
    CHECK(t.look(20).sky_top.r == 10);
    CHECK(t.look(300).sky_top.r == t.look(0).sky_top.r); // look() wraps too
    CHECK(t.look(-1).sky_top.r == t.look(299).sky_top.r);
}

void test_theme_mixing() {
    using namespace racer;
    RoadTheme a, b;
    a.sky_top = Color{0, 0, 0};
    b.sky_top = Color{200, 100, 50};
    a.fog_density = 4.f;
    b.fog_density = 8.f;
    const RoadTheme m0 = mix_themes(a, b, 0.f);
    const RoadTheme m1 = mix_themes(a, b, 1.f);
    const RoadTheme mh = mix_themes(a, b, 0.5f);
    CHECK(m0.sky_top.r == 0 && m1.sky_top.r == 200);
    CHECK(mh.sky_top.r == 100 && mh.sky_top.g == 50 && mh.sky_top.b == 25);
    CHECK_NEAR(mh.fog_density, 6.f, 1e-6f);
    CHECK_NEAR(m1.fog_density, 8.f, 1e-6f);
}

void test_edges() {
    using namespace racer;
    Track t;
    t.segments.resize(100);
    // Mimics TrackBuilder::mark(): a cliff run on the left, a rail on the right.
    for (int i = 20; i < 60; ++i) {
        Segment& s = t.segments[static_cast<size_t>(i)];
        s.left = Edge::Cliff;
        s.right = Edge::Rail;
        s.edge_fade = std::clamp(std::min(static_cast<float>(i - 20 + 1), static_cast<float>(60 - i)) / 12.f, 0.f, 1.f);
    }
    // Cliffs taper to nothing at both ends of a run and are tall in the middle.
    CHECK_NEAR(t.edge_height(20, -1), 0.f, 1e-3f);
    CHECK_NEAR(t.edge_height(60, -1), 0.f, 1e-3f);
    CHECK(t.edge_height(21, -1) < t.edge_height(25, -1));
    CHECK(t.edge_height(40, -1) > 0.5f * cliff_height);
    CHECK(t.edge_height(40, -1) < 1.2f * cliff_height);
    CHECK_NEAR(t.edge_height(10, -1), 0.f, 1e-6f); // nothing outside the run
    // Rails keep a constant height, including at the ends.
    CHECK_NEAR(t.edge_height(20, 1), rail_height, 1e-3f);
    CHECK_NEAR(t.edge_height(40, 1), rail_height, 1e-3f);
    CHECK_NEAR(t.edge_height(60, 1), rail_height, 1e-3f);

    // Barrier limits for a car 0.15 road half-widths wide on each side.
    const float inf = std::numeric_limits<float>::infinity();
    CHECK_NEAR(barrier_limit(t.segments[40], 1, 0.15f), rail_offset - 0.15f, 1e-6f);
    CHECK_NEAR(barrier_limit(t.segments[40], -1, 0.15f), cliff_offset - 0.15f, 1e-6f);
    CHECK(barrier_limit(t.segments[10], 1, 0.15f) == inf);
    CHECK(barrier_limit(t.segments[20], -1, 0.15f) == inf); // the cliff has not grown yet
    CHECK(barrier_limit(t.segments[22], -1, 0.15f) == inf);
    CHECK(barrier_limit(t.segments[25], -1, 0.15f) < inf);
}

int changed_pixels(const racer::Framebuffer& a, const racer::Framebuffer& b) {
    int n = 0;
    for (int i = 0; i < a.width() * a.height(); ++i) n += a.pixels()[i] != b.pixels()[i];
    return n;
}

void test_weather() {
    using namespace racer;
    const racer::Color grey{90, 90, 90};
    Framebuffer clear(320, 240), fb(320, 240);
    clear.clear(grey);

    // Nothing falls in fair weather.
    Weather w(320, 240);
    fb.clear(grey);
    w.update(0.f, 0.f, 0.f, 1.f / 60.f);
    w.render(fb);
    CHECK(w.rain_count() == 0 && w.snow_count() == 0);
    CHECK(changed_pixels(clear, fb) == 0);

    // Rain and snow draw something, more of it when heavier.
    int previous = 0;
    for (float level : {0.2f, 0.6f, 1.f}) {
        Weather rain(320, 240);
        rain.update(level, 0.f, 0.f, 1.f / 60.f);
        fb.clear(grey);
        rain.render(fb);
        const int n = changed_pixels(clear, fb);
        CHECK(n > previous);
        previous = n;
    }
    {
        Weather snow(320, 240);
        snow.update(0.f, 1.f, 0.f, 1.f / 60.f);
        fb.clear(grey);
        snow.render(fb);
        CHECK(snow.snow_count() == Weather::max_snow);
        CHECK(changed_pixels(clear, fb) > 100);
        // Snow is lighter than the background, never darker.
        bool darker = false;
        for (int i = 0; i < 320 * 240; ++i) darker = darker || (fb.pixels()[i] & 0xff) < 90;
        CHECK(!darker);
    }

    // Particles stay inside their area however long it runs, and the whole
    // thing is deterministic.
    Weather a(320, 240), b(320, 240);
    for (int i = 0; i < 3000; ++i) {
        const float wind = 30.f * std::sin(static_cast<float>(i) * 0.01f);
        a.update(1.f, 1.f, wind, 1.f / 60.f);
        b.update(1.f, 1.f, wind, 1.f / 60.f);
    }
    Framebuffer fa(320, 240), fb2(320, 240);
    fa.clear(grey);
    fb2.clear(grey);
    a.render(fa);
    b.render(fb2);
    CHECK(changed_pixels(fa, fb2) == 0);
    CHECK(changed_pixels(clear, fa) > 100);

    // reset() restores the initial state.
    a.reset();
    Weather fresh(320, 240);
    Framebuffer f1(320, 240), f2(320, 240);
    f1.clear(grey); f2.clear(grey);
    a.update(1.f, 0.f, 0.f, 0.f);
    fresh.update(1.f, 0.f, 0.f, 0.f);
    a.render(f1);
    fresh.render(f2);
    CHECK(changed_pixels(f1, f2) == 0);
}

void test_start_line() {
    using racer::crossed_line_forward;
    const float L = 1000.f, line = 100.f;
    CHECK(crossed_line_forward(95.f, 105.f, line, L));      // plain crossing
    CHECK(!crossed_line_forward(50.f, 90.f, line, L));      // short of the line
    CHECK(!crossed_line_forward(150.f, 190.f, line, L));    // already past it
    // Stepping backwards, e.g. after bumping a car, is not a crossing ...
    CHECK(!crossed_line_forward(140.f, 99.f, line, L));
    CHECK(!crossed_line_forward(160.f, 120.f, line, L));
    // ... nor is crossing the line backwards.
    CHECK(!crossed_line_forward(110.f, 90.f, line, L));
    // The line is crossed once even when the position wraps at the seam.
    CHECK(crossed_line_forward(990.f, 3.f, 0.f, L));
    CHECK(crossed_line_forward(995.f, 1002.f, 0.f, L));
    // A step that carries the car over the seam and on past a line just behind it.
    CHECK(crossed_line_forward(L - 5.f, 105.f, line, L));
    CHECK(!crossed_line_forward(L - 5.f, 95.f, line, L));
}

void test_lanes() {
    using racer::lane_center;
    CHECK_NEAR(lane_center(3, 0), -2.f / 3.f, 1e-6f);
    CHECK_NEAR(lane_center(3, 1), 0.f, 1e-6f);
    CHECK_NEAR(lane_center(3, 2), 2.f / 3.f, 1e-6f);
    CHECK_NEAR(lane_center(2, 0), -0.5f, 1e-6f);
    CHECK_NEAR(lane_center(2, 1), 0.5f, 1e-6f);

    // Discrete road marking fields come from the nearer theme when mixing.
    racer::RoadTheme eu, us;
    eu.lanes = 3;
    us.lanes = 2;
    us.us_markings = true;
    CHECK(racer::mix_themes(eu, us, 0.3f).lanes == 3);
    CHECK(!racer::mix_themes(eu, us, 0.3f).us_markings);
    CHECK(racer::mix_themes(eu, us, 0.7f).lanes == 2);
    CHECK(racer::mix_themes(eu, us, 0.7f).us_markings);
}

void test_weather_mixing() {
    using namespace racer;
    RoadTheme dry, wet;
    wet.rain = 1.f;
    wet.grip = 0.8f;
    wet.sun_amount = 0.f;
    dry.sun_amount = 1.f;
    const RoadTheme mid = mix_themes(dry, wet, 0.5f);
    CHECK_NEAR(mid.rain, 0.5f, 1e-6f);
    CHECK_NEAR(mid.grip, 0.9f, 1e-6f);
    CHECK_NEAR(mid.sun_amount, 0.5f, 1e-6f);
}

} // namespace

int main() {
    test_deadzone();
    test_pad_mapping();
    test_zones();
    test_theme_mixing();
    test_edges();
    test_weather();
    test_weather_mixing();
    test_lanes();
    test_start_line();

    if (failures) {
        std::fprintf(stderr, "%d check(s) failed\n", failures);
        return 1;
    }
    std::puts("all tests passed");
    return 0;
}
