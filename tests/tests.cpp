// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

// Minimal self-contained unit tests; no framework needed. Run with `ctest`
// or directly: ./kurvenrausch_tests

#include "input.hpp"
#include "track.hpp"

#include <cmath>
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

} // namespace

int main() {
    test_deadzone();
    test_pad_mapping();
    test_zones();
    test_theme_mixing();

    if (failures) {
        std::fprintf(stderr, "%d check(s) failed\n", failures);
        return 1;
    }
    std::puts("all tests passed");
    return 0;
}
