// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

// Minimal self-contained unit tests; no framework needed. Run with `ctest`
// or directly: ./kurvenrausch_tests

#include "drivetrain.hpp"
#include "driving.hpp"
#include "input.hpp"
#include "road.hpp"
#include "synth.hpp"
#include "track.hpp"
#include "weather.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>
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
    {   // Horn on X or the left shoulder, nitro on Y or the right shoulder.
        InputState in;
        racer::merge_pad(in, PadState{});
        CHECK(!in.horn && !in.nitro);
        PadState pad;
        pad.x = true;
        pad.right_shoulder = true;
        racer::merge_pad(in, pad);
        CHECK(in.horn && in.nitro);
        InputState in2;
        PadState pad2;
        pad2.left_shoulder = true;
        pad2.y = true;
        racer::merge_pad(in2, pad2);
        CHECK(in2.horn && in2.nitro);
        // A key held on the keyboard is not released by an idle pad.
        InputState key;
        key.horn = true;
        racer::merge_pad(key, PadState{});
        CHECK(key.horn);
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

// The road renderer looking back, as the rear-view mirror does.
void test_road_mirror() {
    using namespace racer;
    auto make_track = [](float curve) {
        Track t;
        t.segments.resize(400);
        for (Segment& s : t.segments) s.curve = curve;
        Zone z;
        z.country = "C0";
        t.zones.push_back(z);
        t.finish(40);
        return t;
    };
    const SpriteSheet sprites;
    const Color magenta{255, 0, 255};
    Bitmap marker(8, 8);
    for (uint32_t& p : marker.px) p = magenta.argb();

    // Renders into a mirror sized framebuffer without fog, so colours are exact.
    constexpr int w = 112, h = 30;
    Framebuffer fb(w, h);
    auto render = [&](const Track& t, float position, int direction, std::vector<RoadSprite> objects) {
        fb.clear(Color{0, 0, 0});
        RoadView v;
        v.position = position;
        v.camera_height = 700.f;
        v.camera_depth = 1.2f;
        v.fog_density = 0.f;
        v.direction = direction;
        v.horizon = 13.f;
        v.y_scale = 42.f;
        RoadRenderer r;
        r.render(fb, t, v, sprites, objects);
    };
    auto sprite = [&](float z, float offset) {
        RoadSprite s;
        s.z = z;
        s.bitmap = &marker;
        s.offset = offset;
        s.world_width = 600.f;
        return std::vector<RoadSprite>{s};
    };
    // Number and mean column of the pixels of colour c.
    auto find = [&](std::initializer_list<Color> colours, int row0, int row1) {
        int n = 0;
        float sum = 0.f;
        for (int y = row0; y < row1; ++y) {
            for (int x = 0; x < w; ++x) {
                for (Color c : colours) {
                    if (fb.pixels()[y * w + x] == c.argb()) { ++n; sum += static_cast<float>(x); break; }
                }
            }
        }
        return std::make_pair(n, n ? sum / static_cast<float>(n) : -1.f);
    };

    const Track straight = make_track(0.f);
    const float pos = 40100.f;
    // A car behind on the left shows on the left of the mirror, and not ahead.
    render(straight, pos, -1, sprite(pos - 3000.f, -0.5f));
    auto [n_back, x_back] = find({magenta}, 0, h);
    CHECK(n_back > 0);
    CHECK(x_back < w / 2.f);
    render(straight, pos, 1, sprite(pos - 3000.f, -0.5f));
    CHECK(find({magenta}, 0, h).first == 0);
    // And one ahead only in the forward view.
    render(straight, pos, -1, sprite(pos + 3000.f, 0.5f));
    CHECK(find({magenta}, 0, h).first == 0);
    render(straight, pos, 1, sprite(pos + 3000.f, 0.5f));
    auto [n_ahead, x_ahead] = find({magenta}, 0, h);
    CHECK(n_ahead > 0);
    CHECK(x_ahead > w / 2.f);
    // At the same distance, the two look alike: same size, mirrored position.
    CHECK(std::abs(n_ahead - n_back) <= n_ahead / 5);
    CHECK_NEAR(x_ahead - w / 2.f, w / 2.f - x_back, 2.f);

    // Looking back across the lap seam.
    render(straight, 100.f, -1, sprite(straight.length() - 2900.f, 0.f));
    auto [n_seam, x_seam] = find({magenta}, 0, h);
    CHECK(n_seam > 0);
    CHECK_NEAR(x_seam, w / 2.f, 2.f);

    // In a right-hand bend the road curves to the right behind the car as well
    // as ahead of it: the mirror keeps the sides.
    const Track bend = make_track(4.f);
    const RoadTheme& look = bend.look(0);
    render(bend, pos, -1, {});
    const float road_back = find({look.road[0], look.road[1]}, 17, 19).second;
    render(bend, pos, 1, {});
    const float road_ahead = find({look.road[0], look.road[1]}, 17, 19).second;
    CHECK(road_back > w / 2.f + 5.f);
    CHECK(road_ahead > w / 2.f + 5.f);
}

void test_blit_rotated() {
    using namespace racer;
    // A 4x2 bitmap, left half red, right half blue, with one transparent pixel.
    Bitmap b(4, 2);
    const Color red{255, 0, 0}, blue{0, 0, 255};
    for (int y = 0; y < 2; ++y)
        for (int x = 0; x < 4; ++x) b.set(x, y, x < 2 ? red : blue);
    b.px[0] = 0u;
    Framebuffer fb(10, 10);
    const uint32_t black = Color{0, 0, 0}.argb();
    auto at = [&](int x, int y) { return fb.pixels()[y * 10 + x]; };
    // Unrotated it lands where a plain blit would, transparency kept.
    fb.clear(Color{0, 0, 0});
    fb.blit_rotated(b, 5.f, 5.f, 4.f, 2.f, 0.f);
    CHECK(at(3, 4) == black && at(4, 4) == red.argb() && at(6, 5) == blue.argb());
    CHECK(at(2, 4) == black && at(7, 4) == black && at(5, 3) == black);
    // Half a turn swaps the halves; the transparent corner goes bottom right.
    fb.clear(Color{0, 0, 0});
    fb.blit_rotated(b, 5.f, 5.f, 4.f, 2.f, PI);
    CHECK(at(3, 4) == blue.argb() && at(3, 5) == blue.argb() && at(6, 4) == red.argb());
    CHECK(at(6, 5) == black);
    // A quarter turn clockwise stands it up: red on top, the transparent
    // corner top right.
    fb.clear(Color{0, 0, 0});
    fb.blit_rotated(b, 5.f, 5.f, 4.f, 2.f, PI / 2.f);
    CHECK(at(4, 3) == red.argb() && at(4, 4) == red.argb() && at(5, 4) == red.argb());
    CHECK(at(5, 3) == black);
    CHECK(at(4, 6) == blue.argb() && at(5, 6) == blue.argb());
    CHECK(at(3, 5) == black && at(6, 5) == black);
    // Off screen and degenerate sizes are harmless.
    fb.blit_rotated(b, -50.f, 300.f, 4.f, 2.f, 1.f);
    fb.blit_rotated(b, 5.f, 5.f, 0.f, 2.f, 1.f);
}

void test_framebuffer_blit() {
    using namespace racer;
    Framebuffer src(4, 3), dst(10, 10);
    src.clear(Color{255, 0, 0});
    dst.clear(Color{0, 0, 0});
    dst.blit(src, 8, -1); // hangs off the right and the top
    const uint32_t red = Color{255, 0, 0}.argb();
    int n = 0;
    for (int i = 0; i < 100; ++i) n += dst.pixels()[i] == red;
    CHECK(n == 2 * 2);
    CHECK(dst.pixels()[0 * 10 + 8] == red && dst.pixels()[1 * 10 + 9] == red);
    dst.blit(src, -10, 0); // entirely outside
    n = 0;
    for (int i = 0; i < 100; ++i) n += dst.pixels()[i] == red;
    CHECK(n == 4);
}

void test_nitro() {
    using namespace racer;
    Nitro n;
    CHECK(n.canisters() == Nitro::capacity && !n.burning());
    CHECK(n.fire());
    CHECK(n.burning() && n.canisters() == Nitro::capacity - 1);
    CHECK_NEAR(n.burn_left(), 1.f, 1e-6f);
    CHECK(!n.fire()); // one burn at a time
    CHECK(n.canisters() == Nitro::capacity - 1);
    n.update(Nitro::burn_seconds / 2.f);
    CHECK_NEAR(n.burn_left(), 0.5f, 1e-5f);
    CHECK_NEAR(n.intensity(), 1.f, 1e-6f);
    n.update(Nitro::burn_seconds / 2.f - 0.2f);
    CHECK(n.intensity() > 0.f && n.intensity() < 1.f); // fading out
    n.update(0.3f);
    CHECK(!n.burning() && n.intensity() == 0.f);
    // Runs dry, refills.
    CHECK(n.fire());
    n.update(Nitro::burn_seconds);
    CHECK(n.fire());
    n.update(Nitro::burn_seconds);
    CHECK(n.canisters() == 0 && !n.fire());
    n.refill();
    CHECK(n.canisters() == Nitro::capacity);
    n.fire();
    n.reset();
    CHECK(n.canisters() == Nitro::capacity && !n.burning());
}

void test_speed_rules() {
    using namespace racer;
    // Below the top speed, nothing changes.
    CHECK_NEAR(limit_speed(50.f, 60.f, 100.f, 10.f, 0.1f), 60.f, 1e-6f);
    // The engine stops at the top speed.
    CHECK_NEAR(limit_speed(95.f, 105.f, 100.f, 10.f, 0.1f), 100.f, 1e-6f);
    // Above it the car slows down to it, without overshooting, braking still works.
    CHECK_NEAR(limit_speed(120.f, 125.f, 100.f, 10.f, 0.1f), 119.f, 1e-5f);
    CHECK_NEAR(limit_speed(100.5f, 101.f, 100.f, 10.f, 0.1f), 100.f, 1e-6f);
    CHECK_NEAR(limit_speed(120.f, 110.f, 100.f, 10.f, 0.1f), 109.f, 1e-5f);

    // The pass boost adds a little, but not beyond its limit.
    CHECK_NEAR(boosted_speed(50.f, 100.f), 50.f + 100.f * pass_boost, 1e-5f);
    CHECK_NEAR(boosted_speed(100.f, 100.f), 100.f * (1.f + pass_boost), 1e-4f);
    CHECK_NEAR(boosted_speed(105.f, 100.f), 100.f * pass_boost_limit, 1e-4f);
    CHECK_NEAR(boosted_speed(125.f, 100.f), 125.f, 1e-6f); // already faster (nitro)

    CHECK_NEAR(signed_gap(100.f, 300.f, 1000.f), 200.f, 1e-4f);
    CHECK_NEAR(signed_gap(300.f, 100.f, 1000.f), -200.f, 1e-4f);
    CHECK_NEAR(signed_gap(950.f, 50.f, 1000.f), 100.f, 1e-4f);  // across the seam
    CHECK_NEAR(signed_gap(50.f, 950.f, 1000.f), -100.f, 1e-4f);

    const float car = 0.3f;
    CHECK(close_pass(20.f, -5.f, 0.4f, car, 500.f));     // alongside, then behind
    CHECK(close_pass(20.f, -5.f, -0.5f, car, 500.f));    // on either side
    CHECK(!close_pass(20.f, -5.f, 0.7f, car, 500.f));    // too far apart
    CHECK(!close_pass(20.f, 5.f, 0.4f, car, 500.f));     // still ahead
    CHECK(!close_pass(-5.f, -20.f, 0.4f, car, 500.f));   // was already behind
    CHECK(!close_pass(-5.f, 20.f, 0.4f, car, 500.f));    // it passed us
    CHECK(!close_pass(4000.f, -4000.f, 0.f, car, 500.f)); // a jump, not a pass
}

void test_yield_lane() {
    using namespace racer;
    const std::vector<bool> free3(3, false);
    // In the middle lane with the player behind: the nearest free lane that is
    // out of the player's line, ties go to the left.
    CHECK(yield_lane(3, 0.f, 0.f, 0.4f, free3) == 0);
    // Player on the left: move right, and the other way round.
    CHECK(yield_lane(3, -2.f / 3.f, -2.f / 3.f, 0.4f, free3) == 1);
    CHECK(yield_lane(3, 2.f / 3.f, 2.f / 3.f, 0.4f, free3) == 1);
    // Player between two lanes: both are in the way, the far lane is free.
    CHECK(yield_lane(3, 0.f, -1.f / 3.f, 0.4f, free3) == 2);
    // Busy lanes are skipped; nowhere to go gives -1.
    CHECK(yield_lane(3, 0.f, 0.f, 0.4f, {true, false, false}) == 2);
    CHECK(yield_lane(3, 0.f, 0.f, 0.4f, {true, false, true}) == -1);
    CHECK(yield_lane(2, 0.5f, 0.5f, 0.4f, {false, false}) == 0);
    CHECK(yield_lane(2, 0.5f, 0.5f, 0.4f, {true, false}) == -1);
    // Off to one side of the player, a car does not cross in front of them.
    CHECK(yield_lane(3, -0.36f, 0.06f, 0.45f, free3) == 0);
    CHECK(yield_lane(3, -0.36f, 0.06f, 0.45f, {true, false, false}) == -1);
    CHECK(yield_lane(3, 0.4f, 0.f, 0.45f, {false, false, true}) == -1);
    // Nearly in line, it goes to the side it leans to.
    CHECK(yield_lane(3, 0.f, -0.07f, 0.45f, free3) == 2);
    CHECK(yield_lane(3, 0.f, 0.07f, 0.45f, free3) == 0);
}

void test_start_line() {
    using racer::crossed_line_forward;
    const float L = 1000.f, line = 100.f;
    CHECK(crossed_line_forward(95.f, 105.f, line, L));      // plain crossing
    CHECK(!crossed_line_forward(50.f, 90.f, line, L));      // short of the line
    CHECK(!crossed_line_forward(150.f, 190.f, line, L));    // already past it
    // Stepping backwards, e.g. after bumping a car, is not a crossing ...
    CHECK(!crossed_line_forward(140.f, 100.f - 1.f, line, L));
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

// Invariants of the real route that the renderer and the physics rely on.
void test_demo_track() {
    using namespace racer;
    const Track t = build_demo_track();
    const int n = static_cast<int>(t.segments.size());

    CHECK(t.zones.size() == 6);
    CHECK(t.zones.front().first_segment == 0);
    CHECK((int)t.zone_index.size() == n && (int)t.looks.size() == n);
    for (size_t k = 1; k < t.zones.size(); ++k) {
        const int length = t.zones[k].first_segment - t.zones[k - 1].first_segment;
        CHECK(length > 200); // longer than the blend between zones
    }
    CHECK(n - t.zones.back().first_segment > 200);

    // The lap closes: no jump in height at the seam, and every segment joins the next.
    CHECK_NEAR(t.segments.front().y1, 0.f, 1e-3f);
    CHECK_NEAR(t.segments.back().y2, 0.f, 1.f);
    for (int i = 1; i < n; ++i) {
        CHECK_NEAR(t.segments[static_cast<size_t>(i)].y1, t.segments[static_cast<size_t>(i) - 1].y2, 1e-3f);
    }

    for (int i = 0; i < n; ++i) {
        const Segment& s = t.segments[static_cast<size_t>(i)];
        const RoadTheme& look = t.look(i);
        CHECK(look.lanes == 2 || look.lanes == 3);
        CHECK(look.grip > 0.3f && look.grip <= 1.f);
        // Nothing is planted on a side that carries a rail or cliff.
        for (const RoadsideObject& o : s.scenery) {
            if (o.kind == Scenery::Gantry) continue;
            CHECK(!(o.offset < 0.f && s.left != Edge::None));
            CHECK(!(o.offset > 0.f && s.right != Edge::None));
            // Landmarks and solid objects must stay clear of the road itself.
            CHECK(std::abs(o.offset) >= 1.1f);
        }
    }

    // The route has cliffs, rails and all the weather the zones promise.
    bool cliff = false, rail = false;
    float max_rain = 0.f, max_snow = 0.f;
    for (int i = 0; i < n; ++i) {
        const Segment& s = t.segments[static_cast<size_t>(i)];
        cliff = cliff || s.left == Edge::Cliff || s.right == Edge::Cliff;
        rail = rail || s.left == Edge::Rail || s.right == Edge::Rail;
        max_rain = std::max(max_rain, t.look(i).rain);
        max_snow = std::max(max_snow, t.look(i).snowfall);
    }
    CHECK(cliff && rail);
    CHECK(max_rain > 0.5f && max_snow > 0.5f);

    // The start line and its gantry are on the first straight.
    CHECK(t.segments[8].checker && t.segments[9].checker);
    CHECK(t.zone_number_at(t.start_z) == 0);
}

// ---- Audio -----------------------------------------------------------------

using Samples = std::vector<int16_t>;

// Renders `seconds` of sound with constant parameters; optionally a crash after
// `crash_at` seconds.
// Renders `seconds` of sound; at `event_at` a crash (or a whoosh) is triggered.
Samples render_sound(const racer::SynthParams& p, double seconds, double event_at = -1.0, bool whoosh = false) {
    racer::Synth synth;
    synth.set_params(p);
    Samples out(static_cast<size_t>(seconds * racer::Synth::sample_rate));
    const size_t event_sample = event_at < 0 ? out.size() : static_cast<size_t>(event_at * racer::Synth::sample_rate);
    size_t done = 0;
    while (done < out.size()) {
        const size_t block = std::min<size_t>(735, out.size() - done);
        if (event_sample >= done && event_sample < done + block) {
            if (whoosh) synth.trigger_whoosh(1.f);
            else synth.trigger_crash(1.f);
        }
        synth.render(out.data() + done, static_cast<int>(block));
        done += block;
    }
    return out;
}

// Signal power at one frequency (Goertzel).
double power_at(const Samples& x, size_t from, size_t n, double hz) {
    const double w = 2.0 * 3.14159265358979 * hz / racer::Synth::sample_rate;
    const double coeff = 2.0 * std::cos(w);
    double s1 = 0, s2 = 0;
    for (size_t i = from; i < from + n; ++i) {
        const double hann = 0.5 - 0.5 * std::cos(2.0 * 3.14159265358979 * static_cast<double>(i - from) / static_cast<double>(n));
        const double s0 = hann * x[i] + coeff * s1 - s2;
        s2 = s1;
        s1 = s0;
    }
    return s1 * s1 + s2 * s2 - coeff * s1 * s2;
}

// Power summed over a band, after skipping the first half second (settling).
double band_power(const Samples& x, double lo, double hi) {
    const size_t from = racer::Synth::sample_rate / 2;
    const size_t n = std::min<size_t>(x.size() - from, 32768);
    double sum = 0;
    for (double hz = lo; hz < hi; hz += 25.0) sum += power_at(x, from, n, hz);
    return sum;
}

double rms(const Samples& x, size_t from, size_t to) {
    double sum = 0;
    for (size_t i = from; i < to; ++i) sum += static_cast<double>(x[i]) * x[i];
    return std::sqrt(sum / static_cast<double>(to - from)) / 32768.0;
}

int peak(const Samples& x) {
    int m = 0;
    for (int16_t v : x) m = std::max(m, std::abs(static_cast<int>(v)));
    return m;
}

racer::SynthParams driving() {
    racer::SynthParams p;
    p.rpm = 0.4f;
    p.throttle = 0.5f;
    p.speed = 0.5f;
    return p;
}

void test_drivetrain() {
    using namespace racer::drivetrain;
    CHECK(gear(0.f) == 1 && gear(0.19f) == 1 && gear(0.21f) == 2);
    CHECK(gear(1.f) == gears && gear(2.f) == gears && gear(-1.f) == 1);
    CHECK_NEAR(rpm(0.f), 0.f, 1e-6f);
    CHECK_NEAR(rpm(1.f), 1.f, 1e-6f);
    CHECK(rpm(0.19f) > 0.9f && rpm(0.21f) < 0.35f); // revs drop on the upshift
    CHECK(rpm(0.05f) < rpm(0.1f) && rpm(0.1f) < rpm(0.15f)); // and climb within a gear
}

void test_synth_basics() {
    using namespace racer;
    // Muted means silent.
    SynthParams muted = driving();
    muted.volume = 0.f;
    CHECK(peak(render_sound(muted, 0.5)) <= 1);

    // Deterministic, and independent of how the rendering is chunked.
    const Samples a = render_sound(driving(), 0.3);
    CHECK(a == render_sound(driving(), 0.3));
    Synth chunked;
    chunked.set_params(driving());
    Samples b(a.size());
    for (size_t i = 0; i < b.size();) {
        const size_t block = std::min<size_t>(1 + (i % 97), b.size() - i);
        chunked.render(b.data() + i, static_cast<int>(block));
        i += block;
    }
    CHECK(a == b);

    // Everything at once stays inside the 16-bit range without clipping hard.
    SynthParams all;
    all.rpm = all.throttle = all.speed = all.skid = all.gravel = all.scrape = all.rain = 1.f;
    all.horn = all.nitro = 1.f;
    const Samples loud = render_sound(all, 2.0, 0.5);
    CHECK(peak(loud) < 31000);
    size_t clipped = 0;
    for (int16_t v : loud) clipped += std::abs(static_cast<int>(v)) >= 29990;
    CHECK(clipped * 200 < loud.size()); // under half a percent
    CHECK(rms(loud, 0, loud.size()) > 0.1); // and clearly audible
}

void test_synth_engine() {
    using namespace racer;
    // The engine's energy sits on harmonics of the firing frequency, which is
    // rpm / 60 * 3 for the six cylinder, and follows the revs.
    const auto fire = [](float rpm01) { return (1000.0 + static_cast<double>(rpm01) * 6500.0) / 60.0 * 3.0; };
    for (float rpm01 : {0.25f, 0.5f, 0.75f}) {
        SynthParams p;
        p.rpm = rpm01;
        p.throttle = 1.f;
        const Samples s = render_sound(p, 1.5);
        const size_t from = Synth::sample_rate / 2, n = 16384;
        const double f = fire(rpm01);
        double on = 0, off = 0;
        for (int k = 1; k <= 3; ++k) {
            on += power_at(s, from, n, k * f);
            off += power_at(s, from, n, (k + 0.5) * f);
        }
        CHECK(on > 8.0 * off);
    }
    {   // Each rev setting has its energy at its own frequency, not at the other's.
        SynthParams lo, hi;
        lo.rpm = 0.25f; hi.rpm = 0.75f; lo.throttle = hi.throttle = 1.f;
        const Samples sl = render_sound(lo, 1.5), sh = render_sound(hi, 1.5);
        const size_t from = Synth::sample_rate / 2, n = 16384;
        CHECK(power_at(sl, from, n, fire(0.25f)) > 5.0 * power_at(sh, from, n, fire(0.25f)));
        CHECK(power_at(sh, from, n, fire(0.75f)) > 5.0 * power_at(sl, from, n, fire(0.75f)));
    }
    {   // Open throttle is brighter than a closed one at the same revs.
        SynthParams on, off;
        on.rpm = off.rpm = 0.5f;
        on.throttle = 1.f;
        off.throttle = 0.f;
        CHECK(band_power(render_sound(on, 1.5), 1500, 5000) > 1.5 * band_power(render_sound(off, 1.5), 1500, 5000));
    }
    {   // It idles when standing, rather than falling silent.
        const Samples idle = render_sound(SynthParams{}, 1.0);
        CHECK(rms(idle, 22050, 44100) > 0.03);
        const size_t from = Synth::sample_rate / 2, n = 16384;
        CHECK(power_at(idle, from, n, fire(0.f)) > 8.0 * power_at(idle, from, n, 1.5 * fire(0.f)));
    }
}

void test_synth_effects() {
    using namespace racer;
    const Samples base = render_sound(driving(), 1.5);

    {   // Tyre squeal: a narrow whistle around 1.2 kHz.
        SynthParams p = driving();
        p.speed = 0.8f;
        const Samples plain = render_sound(p, 1.5);
        p.skid = 1.f;
        const Samples skid = render_sound(p, 1.5);
        CHECK(band_power(skid, 800, 1700) > 4.0 * band_power(plain, 800, 1700));
        CHECK(band_power(skid, 900, 1500) > 0.5 * band_power(skid, 300, 3000));
    }
    {   // Gravel crackles in the highs.
        SynthParams p = driving();
        p.gravel = 1.f;
        CHECK(band_power(render_sound(p, 1.5), 1500, 6000) > 3.0 * band_power(base, 1500, 6000));
    }
    {   // Scraping metal is bright and loud.
        SynthParams p = driving();
        p.scrape = 1.f;
        const Samples scrape = render_sound(p, 1.5);
        CHECK(band_power(scrape, 500, 3000) > 2.0 * band_power(base, 500, 3000));
        CHECK(band_power(scrape, 3000, 10000) > 5.0 * band_power(base, 3000, 10000));
    }
    {   // Rain hisses in the top end.
        SynthParams p = driving();
        p.rain = 1.f;
        CHECK(band_power(render_sound(p, 1.5), 4000, 10000) > 2.0 * band_power(base, 4000, 10000));
    }
    {   // Speed adds road roar (lows) and wind (highs) on top of the same engine.
        SynthParams slow = driving(), fast = driving();
        slow.speed = 0.f;
        fast.speed = 1.f;
        const Samples a = render_sound(slow, 1.5), b = render_sound(fast, 1.5);
        CHECK(band_power(b, 100, 500) > 4.0 * band_power(a, 100, 500));
        CHECK(band_power(b, 2000, 8000) > 8.0 * band_power(a, 2000, 8000));
        CHECK(rms(b, 22050, 66150) > 1.1 * rms(a, 22050, 66150));
    }
    {   // A crash is a loud burst that dies away within a couple of seconds.
        const Samples crash = render_sound(driving(), 3.0, 1.0);
        const size_t sr = Synth::sample_rate;
        const double before = rms(crash, sr * 3 / 4, sr);
        const double burst = rms(crash, sr, sr + sr * 3 / 20);
        const double later = rms(crash, sr * 5 / 2, sr * 3);
        CHECK(burst > 2.2 * before);
        CHECK(later < 1.3 * before);
    }
    {   // The horn is a chord of two tones, 415 and 523 Hz.
        SynthParams p = driving();
        p.horn = 1.f;
        const Samples horn = render_sound(p, 1.5);
        const size_t from = Synth::sample_rate / 2, n = 16384;
        for (double f : {415.0, 523.0}) CHECK(power_at(horn, from, n, f) > 10.0 * power_at(base, from, n, f));
        CHECK(power_at(horn, from, n, 415.0) > 10.0 * power_at(horn, from, n, 470.0));
    }
    {   // Nitro roars in the lows.
        SynthParams p = driving();
        p.nitro = 1.f;
        CHECK(band_power(render_sound(p, 1.5), 40, 700) > 2.0 * band_power(base, 40, 700));
    }
    {   // A whoosh is a short burst that is gone within a second.
        const Samples w = render_sound(driving(), 3.0, 1.0, true);
        const size_t sr = Synth::sample_rate;
        const double before = rms(w, sr * 3 / 4, sr);
        const double burst = rms(w, sr + sr / 25, sr + sr / 5);
        const double later = rms(w, sr * 2, sr * 3);
        CHECK(burst > 1.5 * before);
        CHECK(later < 1.2 * before);
        // It is a hiss in the mids, not a thump.
        const Samples early = render_sound(driving(), 1.5, 0.55, true);
        CHECK(band_power(early, 1000, 3000) > 2.0 * band_power(base, 1000, 3000));
    }
}

void test_wav() {
    const std::string path = "kurvenrausch_test.wav";
    const Samples data = {0, 1000, -1000, 32767, -32768};
    CHECK(racer::write_wav(path, data, 44100));
    std::FILE* f = std::fopen(path.c_str(), "rb");
    CHECK(f != nullptr);
    if (f) {
        unsigned char h[44];
        CHECK(std::fread(h, 1, 44, f) == 44);
        CHECK(std::string(reinterpret_cast<char*>(h), 4) == "RIFF" && std::string(reinterpret_cast<char*>(h) + 8, 4) == "WAVE");
        CHECK(h[22] == 1);                                // mono
        CHECK(h[24] == 0x44 && h[25] == 0xAC);            // 44100 Hz
        CHECK(h[34] == 16);                               // bits per sample
        CHECK(h[40] == 10 && h[41] == 0);                 // data bytes
        int16_t back[5];
        CHECK(std::fread(back, 2, 5, f) == 5);
        CHECK(back[1] == 1000 && back[3] == 32767 && back[4] == -32768);
        std::fclose(f);
        std::remove(path.c_str());
    }
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
    test_nitro();
    test_speed_rules();
    test_yield_lane();
    test_road_mirror();
    test_framebuffer_blit();
    test_blit_rotated();
    test_demo_track();
    test_drivetrain();
    test_synth_basics();
    test_synth_engine();
    test_synth_effects();
    test_wav();

    if (failures) {
        std::fprintf(stderr, "%d check(s) failed\n", failures);
        return 1;
    }
    std::puts("all tests passed");
    return 0;
}
