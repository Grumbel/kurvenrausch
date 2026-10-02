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
#include "vehicles.hpp"
#include "people.hpp"
#include "sprites.hpp"
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
    {   // Horn on X, nitro on Y or the right shoulder, handbrake on the left shoulder.
        InputState in;
        racer::merge_pad(in, PadState{});
        CHECK(!in.horn && !in.nitro && !in.handbrake);
        PadState pad;
        pad.x = true;
        pad.right_shoulder = true;
        racer::merge_pad(in, pad);
        CHECK(in.horn && in.nitro && !in.handbrake);
        InputState in2;
        PadState pad2;
        pad2.left_shoulder = true;
        pad2.y = true;
        racer::merge_pad(in2, pad2);
        CHECK(!in2.horn && in2.nitro && in2.handbrake);
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
    w.update(0.f, 0.f, 0.f, 0.f, 1.f / 60.f);
    w.render(fb);
    CHECK(w.rain_count() == 0 && w.snow_count() == 0);
    CHECK(changed_pixels(clear, fb) == 0);

    // Rain and snow draw something, more of it when heavier.
    int previous = 0;
    for (float level : {0.2f, 0.6f, 1.f}) {
        Weather rain(320, 240);
        rain.update(level, 0.f, 0.f, 0.f, 1.f / 60.f);
        fb.clear(grey);
        rain.render(fb);
        const int n = changed_pixels(clear, fb);
        CHECK(n > previous);
        previous = n;
    }
    {
        Weather snow(320, 240);
        snow.update(0.f, 1.f, 0.f, 0.f, 1.f / 60.f);
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
        a.update(1.f, 1.f, wind, 0.f, 1.f / 60.f);
        b.update(1.f, 1.f, wind, 0.f, 1.f / 60.f);
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
    a.update(1.f, 0.f, 0.f, 0.f, 0.f);
    fresh.update(1.f, 0.f, 0.f, 0.f, 0.f);
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

// Road width per zone: it blends between zones, and the road is drawn
// accordingly.
void test_road_width() {
    using namespace racer;
    Track t;
    t.segments.resize(300);
    for (int i = 0; i < 3; ++i) {
        Zone z;
        z.country = "C" + std::to_string(i);
        z.theme.road_scale = i == 1 ? 0.5f : 1.f;
        z.theme.fog_density = 0.f;
        z.first_segment = i * 100;
        t.zones.push_back(z);
    }
    t.finish(40);
    CHECK_NEAR(t.half_width(50), t.road_width, 1e-3f);
    CHECK_NEAR(t.half_width(150), 0.5f * t.road_width, 1e-3f);
    float previous = t.half_width(80);
    for (int i = 81; i < 120; ++i) {
        CHECK(t.half_width(i) <= previous); // narrows smoothly
        previous = t.half_width(i);
    }
    const float z = 150.5f * t.segment_length;
    CHECK(t.half_width_at(z) >= std::min(t.half_width(150), t.half_width(151)) - 1e-3f);
    CHECK(t.half_width_at(z) <= std::max(t.half_width(150), t.half_width(151)) + 1e-3f);

    // Drawn: count the road's pixels across a row where all of it fits.
    const SpriteSheet sprites;
    auto road_pixels = [&](float position) {
        Framebuffer fb(320, 240);
        fb.clear(Color{0, 0, 0});
        RoadView v;
        v.position = position;
        v.player_z = 840.f;
        v.fog_density = 0.f;
        std::vector<RoadSprite> none;
        RoadRenderer r;
        r.render(fb, t, v, sprites, none);
        const RoadTheme& look = t.look(static_cast<int>(position / t.segment_length) + 10);
        int count = 0;
        for (int x = 0; x < 320; ++x) {
            const uint32_t p = fb.pixels()[140 * 320 + x];
            count += p == look.road[0].argb() || p == look.road[1].argb() || p == look.lane.argb();
        }
        return count;
    };
    const int wide = road_pixels(40.f * t.segment_length), narrow = road_pixels(140.f * t.segment_length);
    CHECK(wide > 0 && narrow > 0);
    CHECK(std::abs(static_cast<float>(narrow) / static_cast<float>(wide) - 0.5f) < 0.1f);
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

void test_fuel() {
    using racer::Fuel;
    Fuel f;
    CHECK(f.full() && !f.empty());
    // Idling burns less than full throttle at high revs, which burns a tank
    // in tank_seconds.
    CHECK(Fuel::load(0.f, 0.f) > 0.f);
    CHECK(Fuel::load(0.f, 0.f) < 0.5f * Fuel::load(1.f, 0.5f));
    CHECK_NEAR(Fuel::load(1.f, 1.f), 1.f, 1e-6f);
    f.burn(1.f, Fuel::tank_seconds / 2.f);
    CHECK_NEAR(f.level(), 0.5f, 1e-5f);
    f.burn(1.f, Fuel::tank_seconds);
    CHECK(f.empty() && f.level() == 0.f); // never below empty
    // The pump fills it in fill_seconds, never above full.
    f.refuel(Fuel::fill_seconds / 4.f);
    CHECK_NEAR(f.level(), 0.25f, 1e-5f);
    f.refuel(Fuel::fill_seconds);
    CHECK(f.full() && f.level() == 1.f);
    f.set(0.1f);
    CHECK_NEAR(f.level(), 0.1f, 1e-6f);
    f.reset();
    CHECK(f.full());
}

void test_reverse() {
    using namespace racer;
    // Braking to a stop does not reverse; letting go arms the reverse gear,
    // and only pressing the brake again drives backwards.
    bool armed = false;
    armed = reverse_armed(armed, 100.f, 0.f, 1.f);
    CHECK(!armed && !in_reverse(100.f, 0.f, 1.f, armed));
    armed = reverse_armed(armed, 0.f, 0.f, 1.f); // stopped, brake still held
    CHECK(!armed && !in_reverse(0.f, 0.f, 1.f, armed));
    armed = reverse_armed(armed, 0.f, 0.f, 0.f); // let go
    CHECK(armed && !in_reverse(0.f, 0.f, 0.f, armed));
    armed = reverse_armed(armed, 0.f, 0.f, 1.f); // pressed again
    CHECK(armed && in_reverse(0.f, 0.f, 1.f, armed));
    CHECK(!in_reverse(0.f, 1.f, 1.f, armed)); // not with the throttle too
    CHECK(in_reverse(-50.f, 1.f, 0.f, false)); // rolling backwards, whatever the pedals
    CHECK(!reverse_armed(true, 10.f, 1.f, 0.f)); // driving off forwards disarms it
    // Letting go of the throttle while standing arms it too, but only with
    // the brake released.
    CHECK(!reverse_armed(false, 0.f, 0.f, 0.5f));
    const float accel = 1000.f, stop = 4000.f, top = 1800.f, dt = 1.f / 60.f;
    float v = 0.f;
    for (int i = 0; i < 600; ++i) v = reverse_speed(v, 0.f, 1.f, accel, stop, top, dt);
    CHECK_NEAR(v, -top, 1e-3f); // backwards, up to the reverse top speed
    float braked = v, coasted = v;
    for (int i = 0; i < 10; ++i) {
        braked = reverse_speed(braked, 1.f, 0.f, accel, stop, top, dt);
        coasted = reverse_speed(coasted, 0.f, 0.f, accel, stop, top, dt);
    }
    CHECK(braked > coasted && coasted > v); // the throttle stops it quicker than letting go
    for (int i = 0; i < 200; ++i) braked = reverse_speed(braked, 1.f, 0.f, accel, stop, top, dt);
    CHECK(braked == 0.f); // to a stop, not on into forwards
}

void test_dirt() {
    using namespace racer;
    Dirt d;
    CHECK(d.clean());
    d.splash(0.f, 1.f); // standing in a puddle: nothing thrown up
    CHECK(d.clean());
    d.splash(1.f, 0.5f);
    CHECK(std::abs(d.mud() - Dirt::puddle_rate * 0.5f) < 1e-5f && d.oil() == 0.f);
    d.oil(10.f);
    CHECK(d.oil() == 1.f); // capped
    d.crash(1.f);
    CHECK(std::abs(d.mud() - (Dirt::puddle_rate * 0.5f + Dirt::crash_mud)) < 1e-5f);
    d.wash(Dirt::wash_seconds * 0.5f);
    CHECK(!d.clean() && d.oil() == 0.5f);
    d.wash(Dirt::wash_seconds);
    CHECK(d.clean() && d.mud() == 0.f);
    d.set(2.f, -1.f);
    CHECK(d.mud() == 1.f && d.oil() == 0.f);

    // The overlay: none when clean, more of the car covered the dirtier it
    // is, mud thickest low down, and transparent pixels left alone.
    const Bitmap car = make_car(CarStyle{{0x80, 0x10, 0x10}, {0xc0, 0x20, 0x20}, {0xf0, 0x60, 0x60}, false}, 0);
    const auto changed = [&](float mud, float oil, int y0, int y1) {
        Bitmap b = car;
        apply_dirt(b, mud, oil);
        int n = 0;
        for (int y = y0; y < y1; ++y) {
            for (int x = 0; x < b.w; ++x) {
                const size_t i = static_cast<size_t>(y) * b.w + x;
                CHECK((b.px[i] >> 24) == (car.px[i] >> 24));
                n += b.px[i] != car.px[i];
            }
        }
        return n;
    };
    CHECK(changed(0.f, 0.f, 0, car.h) == 0);
    const int some = changed(0.3f, 0.f, 0, car.h), lots = changed(1.f, 0.f, 0, car.h);
    CHECK(some > 0 && lots > 2 * some);
    CHECK(changed(0.5f, 0.f, car.h / 2, car.h) > changed(0.5f, 0.f, 0, car.h / 2));
    CHECK(changed(0.f, 0.8f, 0, car.h) > 0);
}

void test_lots() {
    using namespace racer;
    const Track t = build_demo_track();
    // How many of each there are, and the building and sign each has.
    const struct {
        Lot kind;
        size_t count;
        Scenery building, sign;
    } expected[] = {
        {Lot::Dealer, 2, Scenery::Showroom, Scenery::DealerSign},
        {Lot::Wash, 3, Scenery::CarWash, Scenery::WashSign},
        {Lot::Motel, 2, Scenery::Motel, Scenery::MotelSign},
        {Lot::Hospital, 2, Scenery::Hospital, Scenery::HospitalSign},
        {Lot::Truckstop, 2, Scenery::Truckstop, Scenery::TruckSign},
    };
    for (const auto& e : expected) {
        const std::vector<int> lots = t.lots(e.kind);
        CHECK(lots.size() == e.count);
        for (int s : lots) {
            int len = 0;
            while (t.segment(s + len).forecourt >= forecourt_width) {
                CHECK(t.segment(s + len).lot == e.kind && t.segment(s + len).curve == 0.f);
                ++len;
            }
            CHECK(len > 30);
            bool building = false, sign = false;
            for (int i = s - 10; i < s + len; ++i) {
                for (const RoadsideObject& o : t.segment(i).scenery) {
                    building = building || o.kind == e.building;
                    sign = sign || o.kind == e.sign;
                }
            }
            CHECK(building && sign);
        }
    }
    for (int k = 0; k < lot_kinds; ++k) CHECK(std::string(lot_name(static_cast<Lot>(k))).size() > 0);
}

void test_pause_menu() {
    using namespace racer;
    PauseMenu m;
    m.open(18, 16); // out of range: wraps
    CHECK(m.selected == PauseMenu::Resume && m.zone == 2 && m.zones == 16);
    CHECK(m.update(MenuInput{}) == MenuAction::None);
    MenuInput up; up.up = true;
    MenuInput down; down.down = true;
    MenuInput left; left.left = true;
    MenuInput right; right.right = true;
    MenuInput ok; ok.confirm = true;
    MenuInput back; back.back = true;
    CHECK(m.update(ok) == MenuAction::Resume);
    m.update(up); // wraps round to the last item
    CHECK(m.selected == PauseMenu::Quit && m.update(ok) == MenuAction::Quit);
    m.update(right); // only changes the country on its own line
    CHECK(m.zone == 2);
    m.update(down);
    m.update(down);
    CHECK(m.selected == PauseMenu::Restart && m.update(ok) == MenuAction::Restart);
    m.update(down);
    m.update(left);
    m.update(left);
    m.update(left);
    CHECK(m.zone == 15);
    m.update(right);
    CHECK(m.zone == 0 && m.update(ok) == MenuAction::StartZone);
    CHECK(m.update(back) == MenuAction::Resume);
    m.open(-1, 16);
    CHECK(m.zone == 0 && m.selected == PauseMenu::Resume);
}

void test_steer_rate() {
    using racer::steer_rate;
    // Nothing when standing, more the faster, as before from half speed up.
    CHECK(steer_rate(0.f) == 0.f);
    CHECK_NEAR(steer_rate(0.5f), 1.f, 1e-5f);
    CHECK_NEAR(steer_rate(1.f), 2.f, 1e-5f);
    CHECK_NEAR(steer_rate(1.3f), 2.6f, 1e-5f); // nitro
    float previous = 0.f;
    for (float s = 0.01f; s <= 1.f; s += 0.01f) {
        CHECK(steer_rate(s) > previous);
        previous = steer_rate(s);
    }
    // Slow, it manoeuvres much better than in proportion to the speed.
    CHECK(steer_rate(0.1f) > 2.f * (2.f * 0.1f));
    CHECK(steer_rate(0.05f) > 3.f * (2.f * 0.05f));
    // Continuous at the knee.
    CHECK_NEAR(steer_rate(0.4999f), steer_rate(0.5001f), 1e-3f);
}

void test_follow_speed() {
    using racer::follow_speed;
    // Far behind it closes in, but no faster than it cruises.
    CHECK_NEAR(follow_speed(100.f, 50.f, 1000.f, 400.f, 1.5f), 100.f, 1e-4f);
    CHECK_NEAR(follow_speed(100.f, 50.f, 420.f, 400.f, 1.5f), 80.f, 1e-4f);
    // At the gap it matches the leader, closer it drops back.
    CHECK_NEAR(follow_speed(100.f, 50.f, 400.f, 400.f, 1.5f), 50.f, 1e-4f);
    CHECK(follow_speed(100.f, 50.f, 380.f, 400.f, 1.5f) < 50.f);
    // Behind a stopped car it stops short of it, never reversing.
    CHECK_NEAR(follow_speed(100.f, 0.f, 400.f, 400.f, 1.5f), 0.f, 1e-4f);
    CHECK_NEAR(follow_speed(100.f, 0.f, 100.f, 400.f, 1.5f), 0.f, 1e-4f);
}

void test_vertical() {
    using namespace racer;
    const float dt = 1.f / 60.f, seg = 200.f;
    // Drives a road made of straight grades (one per segment) at a constant
    // speed; returns the number of steps in the air and the hardest landing.
    auto drive = [&](const std::vector<float>& grades, float speed) {
        Vertical v;
        v.vy = grades[0] * speed; // already moving with the road
        int air = 0;
        float hardest = 0.f;
        float z = 0.f;
        auto road = [&](float at, float& slope) {
            float y = 0.f;
            size_t i = 0;
            for (; i < grades.size() && at >= seg; ++i, at -= seg) y += grades[i] * seg;
            slope = grades[std::min(i, grades.size() - 1)];
            return y + slope * at;
        };
        // Each step moves the car on, then looks at the road where it is now.
        for (z = speed * dt; z < seg * static_cast<float>(grades.size() - 2); z += speed * dt) {
            float slope = 0.f;
            const float y = road(z, slope);
            hardest = std::max(hardest, step_vertical(v, y, slope * speed, jump_gravity, dt));
            air += v.airborne;
            CHECK(v.y >= y - 1e-2f); // never below the road
        }
        return std::make_pair(air, hardest);
    };
    std::vector<float> flat(60, 0.f);
    CHECK(drive(flat, 12000.f).first == 0);
    // A steep climb onto a flat crossing: at speed the car takes off and
    // lands again, at a crawl it stays on the road.
    std::vector<float> crest(30, 0.5f);
    crest.resize(80, 0.f);
    const auto [air_fast, impact_fast] = drive(crest, 12000.f);
    CHECK(air_fast > 10 && air_fast < 60);
    CHECK(impact_fast > 0.f);
    CHECK(drive(crest, 2000.f).first == 0);
    // Dropping into a valley and up the other side keeps it on the road.
    std::vector<float> valley(30, -0.5f);
    valley.resize(80, 0.5f);
    CHECK(drive(valley, 12000.f).first <= 1);
    // Over the crossing and down the next hill it flies further and lands
    // harder than onto the flat.
    std::vector<float> over(30, 0.5f);
    over.resize(36, 0.f);
    over.resize(100, -0.5f);
    const auto [air_down, impact_down] = drive(over, 12000.f);
    CHECK(air_down > air_fast);
    CHECK(impact_down > impact_fast);
}

void test_crash_pose() {
    using namespace racer;
    // Starts and ends on the ground, upright and in place.
    const CrashPose start = crash_pose(0.f, 1);
    CHECK_NEAR(start.angle, 0.f, 1e-6f);
    CHECK_NEAR(start.lift, 0.f, 1e-6f);
    CHECK_NEAR(start.slide, 0.f, 1e-6f);
    const CrashPose end = crash_pose(crash_seconds, 1);
    CHECK_NEAR(std::remainder(end.angle, 2.f * PI), 0.f, 1e-4f);
    CHECK_NEAR(end.lift, 0.f, 1e-6f);
    CHECK_NEAR(end.slide, 0.f, 1e-4f);
    CHECK_NEAR(end.recover, 1.f, 1e-6f);
    CHECK(end.visible);
    // Up in the air during the first hop, higher than during the last.
    CHECK(crash_pose(0.3f, 1).lift > 40.f);
    CHECK(crash_pose(0.3f, 1).lift > 3.f * crash_pose(1.3f, 1).lift);
    // Rolls over twice, in the direction of the crash side.
    CHECK(crash_pose(0.5f, 1).angle > 0.f);
    CHECK(crash_pose(0.5f, -1).angle < 0.f);
    CHECK_NEAR(crash_pose(crash_tumble_seconds - 1e-4f, 1).angle, 4.f * PI, 1e-2f);
    // Slides outwards, rests upright, then is put back.
    CHECK(crash_pose(1.8f, -1).slide < -20.f);
    CHECK_NEAR(crash_pose(1.8f, 1).angle, 0.f, 1e-6f);
    CHECK_NEAR(crash_pose(1.8f, 1).lift, 0.f, 1e-6f);
    CHECK(crash_pose(1.8f, 1).recover == 0.f);
    CHECK(crash_pose(2.4f, 1).recover > 0.f && crash_pose(2.4f, 1).recover < 1.f);
    int hidden = 0;
    for (float t = crash_recover_start; t < crash_seconds; t += 0.01f) hidden += !crash_pose(t, 1).visible;
    CHECK(hidden > 10); // blinks
    // Three touchdowns, each counted once however the time is stepped.
    CHECK(crash_landings(0.f, crash_seconds) == 3);
    int n = 0;
    for (float t = 0.f; t < crash_seconds; t += 1.f / 60.f) n += crash_landings(t, t + 1.f / 60.f);
    CHECK(n == 3);
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

void test_gas_stations() {
    using namespace racer;
    const Track t = build_demo_track();
    const std::vector<int> stations = t.lots(Lot::Gas);
    // One in every zone.
    CHECK(stations.size() == t.zones.size());
    std::vector<int> per_zone(t.zones.size(), 0);
    for (int s : stations) ++per_zone[static_cast<size_t>(t.zone_index[static_cast<size_t>(s)])];
    for (int n : per_zone) CHECK(n == 1);

    for (int s : stations) {
        // The forecourt tapers in, is flat and straight at full width, tapers out.
        CHECK(t.segment(s - 1).forecourt > 1.f && t.segment(s - 1).forecourt < forecourt_width);
        int len = 0;
        while (t.segment(s + len).forecourt >= forecourt_width) {
            const Segment& seg = t.segment(s + len);
            CHECK(seg.curve == 0.f && seg.y1 == seg.y2);
            CHECK(seg.right == Edge::None);
            ++len;
        }
        CHECK(len > 30);
        CHECK(t.forecourt_at(s) <= t.segment(s).forecourt); // the boundary takes the narrower side
        // On the forecourt counts as such; beyond it, or on the road, not.
        const float z = (static_cast<float>(s) + 0.5f) * t.segment_length;
        CHECK(t.on_forecourt(z, 1.5f));
        CHECK(!t.on_forecourt(z, 0.5f) && !t.on_forecourt(z, forecourt_width + 0.1f) && !t.on_forecourt(z, -1.5f));

        // Nothing solid in the way of a car (0.15 half-widths each side) pulling in
        // at 1.45 along the forecourt and its tapers, except the pumps and the
        // shop where they belong; the sign stands before it.
        int pumps = 0, shops = 0, signs = 0;
        for (int i = s - 12; i < s; ++i) {
            for (const RoadsideObject& o : t.segment(i).scenery) {
                if (o.kind != Scenery::FuelSign) continue;
                ++signs;
                CHECK(t.segment(i).forecourt == 0.f);
            }
        }
        CHECK(signs == 1);
        for (int i = s - 5; i < s + len + 5; ++i) {
            for (const RoadsideObject& o : t.segment(i).scenery) {
                if (o.offset <= 0.f) continue;
                pumps += o.kind == Scenery::FuelPump;
                shops += o.kind == Scenery::GasStation;
                CHECK(o.kind == Scenery::FuelPump || o.kind == Scenery::GasStation);
                CHECK(!scenery_info(o.kind).centered && o.offset >= 1.6f);
            }
        }
        CHECK(pumps == 2 && shops == 1);
    }
}

void test_wet_spots() {
    using namespace racer;
    const Track t = build_demo_track();
    const int n = static_cast<int>(t.segments.size());
    std::vector<int> wet(t.zones.size(), 0), length(t.zones.size(), 0);
    for (int i = 0; i < n; ++i) {
        const auto z = static_cast<size_t>(t.zone_index[static_cast<size_t>(i)]);
        ++length[z];
        const Segment& s = t.segment(i);
        if (s.patch != Patch::Water) continue;
        ++wet[z];
        // On the road, clear of the start and the forecourts.
        CHECK(std::abs(s.patch_x) + s.patch_w <= 0.95f + 1e-5f);
        CHECK(i >= 20 && !s.checker && s.forecourt == 0.f);
    }
    // Spots (counted where they start) are where it rains or snows: none in
    // the desert, hardly any in fair weather, several where it is properly
    // wet, and many more per segment in the wet zones than in the dry ones.
    std::vector<int> spots(t.zones.size(), 0);
    for (int i = 1; i < n; ++i) {
        if (t.segment(i).patch == Patch::Water && t.segment(i - 1).patch_w == 0.f)
            ++spots[static_cast<size_t>(t.zone_index[static_cast<size_t>(i)])];
    }
    int wet_spots = 0, wet_length = 0, dry_spots = 0, dry_length = 0;
    for (size_t z = 0; z < t.zones.size(); ++z) {
        const RoadTheme& th = t.zones[z].theme;
        if (t.zones[z].decor == Decor::Desert) CHECK(spots[z] == 0);
        if (th.rain > 0.f || th.snowfall > 0.f) {
            if (std::max(th.rain, th.snowfall) >= 0.5f) CHECK(spots[z] >= 3);
            wet_spots += spots[z];
            wet_length += length[z];
        } else {
            CHECK(spots[z] <= 3);
            dry_spots += spots[z];
            dry_length += length[z];
        }
    }
    CHECK(static_cast<float>(wet_spots) / static_cast<float>(wet_length) >
          10.f * static_cast<float>(dry_spots + 1) / static_cast<float>(dry_length));
    // Oil slicks: rare, but there are some, in fair weather and foul, and
    // never on the same stretch as a puddle (each segment has one patch).
    int slicks = 0;
    for (int i = 1; i < n; ++i) slicks += t.segment(i).patch == Patch::Oil && t.segment(i - 1).patch_w == 0.f;
    CHECK(slicks >= 3 && slicks < wet_spots);
    // Patches (puddles and slicks alike) come to a point at both ends and are
    // widest in the middle.
    for (int i = 1; i < n; ++i) {
        if (t.segment(i).patch_w > 0.f && t.segment(i - 1).patch_w == 0.f) {
            CHECK(t.patch_width_at(i) == 0.f); // starts at a point
            int len = 0;
            while (t.segment(i + len).patch_w > 0.f) ++len;
            CHECK(len >= 4);
            CHECK(t.patch_width_at(i + len) == 0.f);
            CHECK(t.segment(i + len / 2).patch_w > t.segment(i).patch_w);
            // A car touching it counts, one beside it does not.
            const Segment& mid = t.segment(i + len / 2);
            const float z = (static_cast<float>(i + len / 2) + 0.5f) * t.segment_length;
            CHECK(t.patch_under(z, mid.patch_x, 0.15f) == mid.patch);
            CHECK(t.patch_under(z, mid.patch_x + mid.patch_w + 0.1f, 0.15f) == mid.patch);
            CHECK(t.patch_under(z, mid.patch_x + mid.patch_w + 0.2f, 0.15f) == Patch::None);
            CHECK(t.patch_center_at(i + len / 2) == mid.patch_x);
        }
    }
}

void test_san_francisco() {
    using namespace racer;
    const Track t = build_demo_track();
    int sf = -1;
    for (size_t k = 0; k < t.zones.size(); ++k)
        if (t.zones[k].region == "SAN FRANCISCO") sf = static_cast<int>(k);
    CHECK(sf >= 0);
    const int first = t.zones[static_cast<size_t>(sf)].first_segment;
    const int n = static_cast<size_t>(sf) + 1 < t.zones.size() ? t.zones[static_cast<size_t>(sf) + 1].first_segment
                                                               : static_cast<int>(t.segments.size());
    // Huge hills: steep grades both ways, and a big difference in height.
    float steepest_up = 0.f, steepest_down = 0.f, lo = 1e9f, hi = -1e9f;
    for (int i = first; i < n; ++i) {
        const Segment& s = t.segment(i);
        const float grade = (s.y2 - s.y1) / t.segment_length;
        steepest_up = std::max(steepest_up, grade);
        steepest_down = std::min(steepest_down, grade);
        lo = std::min(lo, s.y1);
        hi = std::max(hi, s.y1);
    }
    CHECK(steepest_up >= 0.5f && steepest_down <= -0.5f);
    CHECK(hi - lo > 15.f * t.segment_length);
    // Driven at top speed, the crests throw the car into the air several
    // times; somewhere else on the route they don't.
    auto jumps = [&](int from, int to) {
        const float speed = 60.f * t.segment_length, dt = 1.f / 60.f;
        Vertical v;
        place_on_road(v, t.height_at(static_cast<float>(from) * t.segment_length));
        int count = 0;
        for (float z = static_cast<float>(from) * t.segment_length + speed * dt;
             z < static_cast<float>(to) * t.segment_length; z += speed * dt) {
            const Segment& s = t.segment_at(z);
            const bool was = v.airborne;
            step_vertical(v, t.height_at(z), (s.y2 - s.y1) / t.segment_length * speed, jump_gravity, dt);
            count += v.airborne && !was;
        }
        return count;
    };
    CHECK(jumps(first, n) >= 3);
    for (size_t k = 0; k + 1 < t.zones.size(); ++k) {
        if (t.zones[k].region == "SCHWARZWALD") CHECK(jumps(t.zones[k].first_segment, t.zones[k + 1].first_segment) == 0);
    }
    // City decor: houses and lamps.
    int houses = 0, lamps = 0;
    for (int i = first; i < n; ++i) {
        for (const RoadsideObject& o : t.segment(i).scenery) {
            houses += o.kind == Scenery::Victorian || o.kind == Scenery::VictorianB || o.kind == Scenery::VictorianC;
            lamps += o.kind == Scenery::StreetLamp;
        }
    }
    CHECK(houses > 100 && lamps > 20);
}

void test_branches() {
    using namespace racer;
    Track t = build_demo_track();
    CHECK(t.branches.size() == 2);
    const float lap = t.length();
    const size_t n = t.segments.size();
    for (size_t i = 0; i < t.branches.size(); ++i) {
        const Branch& br = t.branches[i];
        CHECK(!br.names[0].empty() && !br.names[1].empty());
        // Equally long, starting and ending at the height of the road around
        // them, and different courses.
        CHECK(br.routes[0].size() == static_cast<size_t>(br.length) && br.routes[1].size() == br.routes[0].size());
        for (int r = 0; r < 2; ++r) {
            CHECK_NEAR(br.routes[r].front().y1, t.segment(br.fork - 1).y2, 1e-2f);
            CHECK_NEAR(br.routes[r].back().y2, t.segment(br.end()).y1, 1e-2f);
            int planted = 0;
            for (const Segment& s : br.routes[r]) planted += static_cast<int>(s.scenery.size());
            CHECK(planted > 20); // both routes are decorated
            // Facing the other route nothing stands where they part and meet,
            // and elsewhere only what fits between the two roads.
            for (size_t k = 0; k < br.routes[r].size(); ++k) {
                const Segment& s = br.routes[r][k];
                CHECK(s.facing_branch == (r == 0 ? 1 : -1));
                for (const RoadsideObject& o : s.scenery) {
                    if (o.offset * static_cast<float>(s.facing_branch) <= 0.f) continue;
                    CHECK(!s.branch_bend);
                    CHECK(std::abs(o.offset) + scenery_info(o.kind).width / t.half_width(br.fork + static_cast<int>(k)) <= 2.6f + 1e-3f);
                }
            }
            // The S-bends ease in, turn over and ease out: the curvature
            // starts near zero and never jumps.
            CHECK(std::abs(br.routes[r].front().curve) < 0.5f);
            for (size_t k = 1; k < br.routes[r].size(); ++k) {
                if (br.routes[r][k].branch_bend && br.routes[r][k - 1].branch_bend)
                    CHECK(std::abs(br.routes[r][k].curve - br.routes[r][k - 1].curve) < 0.5f);
            }
        }
        float diff = 0.f;
        for (int k = 0; k < br.length; ++k) diff += std::abs(br.routes[0][static_cast<size_t>(k)].curve - br.routes[1][static_cast<size_t>(k)].curve);
        CHECK(diff > 100.f);

        // The other road: on top of ours at the fork, parting smoothly (its
        // heading the same as ours at both ends of the bend), well apart and
        // alongside in between (the left one on the left), and back together
        // where they meet.
        CHECK(br.active == 1);
        CHECK_NEAR(t.branch_offset(br.fork), 0.f, 1e-4f);
        CHECK_NEAR(t.branch_slope(br.fork), 0.f, 1e-4f);
        CHECK(t.branch_slope(br.fork + br.bend / 2) < -50.f);
        CHECK(std::abs(t.branch_slope(br.fork + br.bend)) < 1.f);
        CHECK(t.branch_offset(br.fork + br.bend) < -3.f);
        for (int k = br.fork + br.bend; k <= br.end() - br.bend; k += 7) {
            CHECK_NEAR(t.branch_offset(k), t.branch_offset(br.fork + br.bend), 0.3f);
        }
        CHECK(std::abs(t.branch_offset(br.end())) < 0.05f);
        CHECK(std::isnan(t.branch_offset(br.fork - 1)));
        CHECK(std::isnan(t.branch_offset(br.end() + 1)));

        // Choosing the other route swaps the segments and mirrors the offsets.
        const Segment middle = t.segment(br.fork + br.length / 2);
        t.choose_branch(i, 0);
        CHECK(t.branches[i].active == 0);
        CHECK(t.segment(br.fork + br.bend / 4).curve < 0.f); // the left route bends left
        CHECK(t.branch_offset(br.fork + br.bend) > 3.f);
        CHECK(t.segments.size() == n && t.length() == lap);
        t.choose_branch(i, 1);
        CHECK(t.segment(br.fork + br.length / 2).curve == middle.curve);
        CHECK(t.segment(br.fork + br.length / 2).scenery.size() == middle.scenery.size());
    }
    // The car takes the road it is nearer to.
    CHECK(!nearer_other_road(0.f, -0.1f));
    CHECK(nearer_other_road(-0.3f, -0.4f));
    CHECK(!nearer_other_road(0.2f, -0.3f));
    // Where the roads still coincide it does not flip back and forth, nor
    // when only just nearer the other road.
    CHECK(!nearer_other_road(-0.01f, -0.02f) && !nearer_other_road(0.01f, 0.f));
    CHECK(!nearer_other_road(-0.15f, -0.25f));
}

void test_track_map() {
    using namespace racer;
    auto check_map = [](const Track& t) {
        const std::vector<MapPoint> m = track_map(t);
        CHECK(m.size() == t.segments.size());
        float max_step = 0.f, min_x = 1.f, max_x = 0.f, min_y = 1.f, max_y = 0.f;
        for (size_t i = 0; i < m.size(); ++i) {
            const MapPoint& a = m[i];
            const MapPoint& b = m[(i + 1) % m.size()]; // including the seam: it is a loop
            max_step = std::max(max_step, std::hypot(b.x - a.x, b.y - a.y));
            min_x = std::min(min_x, a.x); max_x = std::max(max_x, a.x);
            min_y = std::min(min_y, a.y); max_y = std::max(max_y, a.y);
        }
        // Inside the unit square, filling it in one direction, centred in the other.
        CHECK(min_x >= 0.f && min_y >= 0.f && max_x <= 1.f + 1e-5f && max_y <= 1.f + 1e-5f);
        CHECK(std::max(max_x - min_x, max_y - min_y) > 0.999f);
        CHECK_NEAR(min_x, 1.f - max_x, 1e-4f);
        CHECK_NEAR(min_y, 1.f - max_y, 1e-4f);
        // No jumps anywhere, so it closes smoothly.
        CHECK(max_step < 20.f / static_cast<float>(m.size()));
    };
    // A plain oval of straights and right-hand bends, and the real route.
    Track oval;
    oval.segments.resize(400);
    for (int i = 0; i < 400; ++i) oval.segments[static_cast<size_t>(i)].curve = (i / 100) % 2 ? 3.f : 0.f;
    check_map(oval);
    check_map(build_demo_track());
    // Right-hand bends turn the map clockwise (y is down): the start heads up,
    // and the next bend swings it to the right.
    const std::vector<MapPoint> m = track_map(oval);
    CHECK(m[100].y < m[0].y);
    CHECK(m[200].x > m[100].x);
}

void test_car_models() {
    using namespace racer;
    // Every model has a name and trades something for its strength: none is
    // at least as good as another in top speed, acceleration and grip at once.
    for (int a = 0; a < car_models; ++a) {
        const CarModel& m = car_model(a);
        CHECK(m.name != nullptr && m.name[0] != '\0');
        CHECK(m.top_speed > 0.8f && m.top_speed < 1.2f);
        for (int b = 0; b < car_models; ++b) {
            if (a == b) continue;
            const CarModel& o = car_model(b);
            CHECK(!(m.top_speed >= o.top_speed && m.acceleration >= o.acceleration && m.grip >= o.grip));
        }
    }
    CHECK(car_model(0).top_speed == 1.f && car_model(0).acceleration == 1.f && car_model(0).grip == 1.f);
    CHECK(&car_model(car_models) == &car_model(0)); // wraps around
    CHECK(&car_model(-1) == &car_model(car_models - 1));
}

void test_vehicles() {
    using namespace racer;
    // Shares add up; picking by them gives every kind, rivals rarely.
    float sum = 0.f;
    int counts[static_cast<int>(Vehicle::Count)] = {};
    for (int i = 0; i < static_cast<int>(Vehicle::Count); ++i) sum += vehicle_info(static_cast<Vehicle>(i)).share;
    CHECK_NEAR(sum, 1.f, 1e-5f);
    for (int i = 0; i < 1000; ++i) ++counts[static_cast<int>(traffic_vehicle((static_cast<float>(i) + 0.5f) / 1000.f))];
    for (int n : counts) CHECK(n > 0);
    CHECK(counts[static_cast<int>(Vehicle::Rival)] < 100);
    CHECK(counts[static_cast<int>(Vehicle::Car)] > counts[static_cast<int>(Vehicle::Truck)]);
    // Trucks are slower than rivals; rivals race only when the player is near.
    CHECK(vehicle_info(Vehicle::Truck).max_speed < vehicle_info(Vehicle::Rival).min_speed);
    CHECK_NEAR(rival_speed(50.f, 100.f, 10.f), 100.f * rival_race_speed, 1e-4f);
    CHECK_NEAR(rival_speed(50.f, 100.f, -2.f), 100.f * rival_race_speed, 1e-4f);
    CHECK_NEAR(rival_speed(50.f, 100.f, 200.f), 50.f, 1e-6f);
    CHECK_NEAR(rival_speed(50.f, 100.f, -200.f), 50.f, 1e-6f);
    // Every style of every kind has its sprites, at the shared pixel scale
    // (6.25 world units per pixel), and the brake lights make a difference.
    const SpriteSheet sheet;
    for (int k = 0; k < static_cast<int>(Vehicle::Count); ++k) {
        const auto kind = static_cast<Vehicle>(k);
        for (int st = 0; st < vehicle_info(kind).styles; ++st) {
            const Bitmap& rear = sheet.vehicle(kind, st);
            CHECK(std::abs(static_cast<float>(rear.w) * 6.25f - vehicle_info(kind).width) < 13.f);
            CHECK(sheet.vehicle_front(kind, st).w == rear.w);
            CHECK(sheet.vehicle(kind, st, 0, true).px != rear.px);
            CHECK(sheet.vehicle(kind, st, -1).px != rear.px);
        }
    }
    CHECK(sheet.vehicle(Vehicle::Truck, 0).h > 2 * sheet.vehicle(Vehicle::Car, 0).h);
    // Tyre tread frames: they differ from each other, only in the tyres (the
    // bottom of the sprite), and cycle with the distance driven.
    for (int k = 0; k < static_cast<int>(Vehicle::Count); ++k) {
        const auto kind = static_cast<Vehicle>(k);
        const Bitmap& f0 = sheet.vehicle(kind, 0, 0, false, 0);
        const Bitmap& f1 = sheet.vehicle(kind, 0, 0, false, 1);
        CHECK(f0.px != f1.px && f1.px != sheet.vehicle(kind, 0, 0, false, 2).px);
        CHECK(sheet.vehicle_front(kind, 0, 0, 0).px != sheet.vehicle_front(kind, 0, 0, 1).px);
        int top = f0.h;
        for (int y = 0; y < f0.h; ++y)
            for (int x = 0; x < f0.w; ++x)
                if (f0.get(x, y) != f1.get(x, y)) top = std::min(top, y);
        CHECK(top > f0.h / 3);
    }
    CHECK(sheet.player(0, 0, false, 0).px != sheet.player(0, 0, false, 1).px);
    // Each car model looks different.
    for (int m = 1; m < car_models; ++m) CHECK(sheet.player(m, 0).px != sheet.player(0, 0).px);
    // The people: drawn over the car, the same size; each passenger and each
    // driver looks different; a wave raises an arm into the headroom.
    const Bitmap& base = sheet.occupants(0, 0, 0, 0, 0, 0);
    CHECK(base.w == sheet.player(0, 0).w && base.h == sheet.player(0, 0).h);
    for (int p = 1; p < passengers; ++p) CHECK(sheet.occupants(0, p, 0, 0, 0, 0).px != base.px);
    for (int d = 1; d < drivers; ++d) CHECK(sheet.occupants(d, 0, 0, 0, 0, 0).px != base.px);
    auto top_row = [](const Bitmap& b) {
        for (int y = 0; y < b.h; ++y)
            for (int x = 0; x < b.w; ++x)
                if (b.get(x, y) >> 24) return y;
        return b.h;
    };
    CHECK(top_row(sheet.occupants(0, 0, 0, -1, 0, 0)) < SpriteSheet::player_headroom);
    CHECK(top_row(base) >= SpriteSheet::player_headroom - 4);
    CHECK(&sheet.occupants(0, 0, 0, 0, 0, 0) == &base); // kept, not made again
    CHECK(sheet.occupants(0, 0, 0, 0, 0, 0, true).px != base.px); // the bandage shows
    CHECK(sheet.occupants(0, 0, 0, 0, 0, 1, true).px != sheet.occupants(0, 0, 0, 0, 0, 1).px);
    // In a closed car the heads only show through the rear window, dimmed.
    int closed = 0, open = 0;
    for (uint32_t px : sheet.occupants(0, 0, 0, 0, 0, 1).px) closed += (px >> 24) != 0;
    for (uint32_t px : base.px) open += (px >> 24) != 0;
    CHECK(closed > 0 && closed < open);
    CHECK(SpriteSheet::tyre_frame(0.f) == 0);
    CHECK(SpriteSheet::tyre_frame(SpriteSheet::tread_step * 1.5f) == 1);
    CHECK(SpriteSheet::tyre_frame(SpriteSheet::tread_step * 2.5f) == 2);
    CHECK(SpriteSheet::tyre_frame(SpriteSheet::tread_step * 3.5f) == 0);
    CHECK(SpriteSheet::tyre_frame(-SpriteSheet::tread_step * 0.5f) == 2);
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

    CHECK(t.zones.size() == 16);
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
    all.horn = all.nitro = all.pump = all.splash = 1.f;
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
    {   // It builds as it revs: much louder near the limit than low down.
        SynthParams lo, hi;
        lo.rpm = 0.25f; hi.rpm = 0.9f; lo.throttle = hi.throttle = 1.f;
        CHECK(rms(render_sound(hi, 1.0), 22050, 44100) > 2.0 * rms(render_sound(lo, 1.0), 22050, 44100));
    }
    {   // The bass stays at high revs, where the firing frequency is far above it:
        // the exhaust pulses ring a low body resonance, and the revolution's
        // lumpiness keeps energy at the revolution rate.
        SynthParams p;
        p.rpm = 0.75f;
        p.throttle = 1.f;
        const Samples s = render_sound(p, 1.5);
        const size_t from = Synth::sample_rate / 2, n = 16384;
        auto band = [&](double lo, double hi) {
            double sum = 0.0;
            for (double f = lo; f < hi; f += static_cast<double>(Synth::sample_rate) / n) sum += power_at(s, from, n, f);
            return sum;
        };
        CHECK(band(30, 200) > 0.3 * band(30, 6000));
    }
    {   // Dynamics: opening the throttle surges past the steady level, and
        // lifting off at revs makes the exhaust crackle for a moment.
        auto steps = [](const SynthParams& a, const SynthParams& b) {
            Synth synth;
            synth.set_params(a);
            Samples out(static_cast<size_t>(Synth::sample_rate) * 5 / 2);
            for (size_t done = 0; done < out.size(); done += 735) {
                if (done == static_cast<size_t>(Synth::sample_rate)) synth.set_params(b);
                synth.render(out.data() + done, static_cast<int>(std::min<size_t>(735, out.size() - done)));
            }
            return out;
        };
        SynthParams off, on;
        off.rpm = on.rpm = 0.8f;
        on.throttle = 1.f;
        const size_t sr = Synth::sample_rate;
        const Samples open = steps(off, on);
        CHECK(rms(open, sr + sr / 20, sr + sr * 3 / 10) > 1.15 * rms(open, sr * 2, sr * 12 / 5));
        const Samples lift = steps(on, off);
        auto highs = [&](size_t from) {
            double sum = 0.0;
            for (double f = 2000.0; f < 6000.0; f += 50.0) sum += power_at(lift, from, 8192, f);
            return sum;
        };
        CHECK(highs(sr + sr / 20) > 3.0 * highs(sr * 2));
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
    {   // The engine falls silent when it is switched off (out of fuel).
        SynthParams on, off;
        on.rpm = off.rpm = 0.5f;
        on.throttle = off.throttle = 1.f;
        off.engine = 0.f;
        const size_t from = Synth::sample_rate / 2, n = 16384;
        const double f = (1000.0 + 0.5 * 6500.0) / 60.0 * 3.0;
        CHECK(power_at(render_sound(on, 1.5), from, n, f) > 100.0 * power_at(render_sound(off, 1.5), from, n, f));
    }
    {   // Water spraying from the tyres hisses in the mids.
        SynthParams p = driving();
        p.splash = 1.f;
        CHECK(band_power(render_sound(p, 1.5), 800, 3000) > 3.0 * band_power(base, 800, 3000));
    }
    {   // The fuel pump hums and gurgles in the lows.
        SynthParams p = driving();
        p.pump = 1.f;
        const Samples pump = render_sound(p, 1.5);
        const size_t from = Synth::sample_rate / 2, n = 16384;
        CHECK(power_at(pump, from, n, 100.0) > 10.0 * power_at(base, from, n, 100.0));
    }
    {   // The chime rings at its two notes and dies away.
        Synth s;
        s.set_params(SynthParams{});
        Samples quiet(Synth::sample_rate), chime(Synth::sample_rate);
        Synth s2;
        s2.set_params(SynthParams{});
        s.render(quiet.data(), static_cast<int>(quiet.size()));
        s2.trigger_ding();
        s2.render(chime.data(), static_cast<int>(chime.size()));
        for (double f : {1318.5, 1046.5}) CHECK(power_at(chime, 0, 16384, f) > 20.0 * power_at(quiet, 0, 16384, f));
        CHECK(rms(chime, chime.size() - 4410, chime.size()) < 1.2 * rms(quiet, quiet.size() - 4410, quiet.size()));
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

void test_weather_vection() {
    using namespace racer;
    const racer::Color grey{90, 90, 90};
    Framebuffer clear(320, 240);
    clear.clear(grey);
    auto run = [](Weather& w, float rain, float snow, float speed) {
        for (int i = 0; i < 300; ++i) w.update(rain, snow, 0.f, speed, 1.f / 60.f);
    };
    // Standing, the precipitation just falls: no stream out of the distance.
    Weather still(320, 240), fast(320, 240);
    run(still, 1.f, 0.f, 0.f);
    run(fast, 1.f, 0.f, 1.f);
    // At speed it streams outwards, faster the faster the car goes.
    CHECK(fast.mean_outflow() > still.mean_outflow() + 150.f);
    Weather half(320, 240);
    run(half, 1.f, 0.f, 0.5f);
    CHECK(half.mean_outflow() > still.mean_outflow() && half.mean_outflow() < fast.mean_outflow());
    // The streaks get longer: the same rain covers more of the screen.
    Framebuffer a(320, 240), b(320, 240);
    a.clear(grey);
    b.clear(grey);
    still.render(a);
    fast.render(b);
    CHECK(changed_pixels(clear, b) > changed_pixels(clear, a) * 3 / 2);
    // Snow streams and smears too.
    Weather snow_still(320, 240), snow_fast(320, 240);
    run(snow_still, 0.f, 1.f, 0.f);
    run(snow_fast, 0.f, 1.f, 1.f);
    CHECK(snow_fast.mean_outflow() > snow_still.mean_outflow() + 150.f);
    a.clear(grey);
    b.clear(grey);
    snow_still.render(a);
    snow_fast.render(b);
    CHECK(changed_pixels(clear, b) > changed_pixels(clear, a));
    // However long it runs at speed, the screen stays covered: the stream is
    // fed from the middle rather than draining away.
    Weather long_run(320, 240);
    for (int i = 0; i < 3000; ++i) long_run.update(1.f, 0.f, 0.f, 1.2f, 1.f / 60.f);
    Framebuffer c(320, 240);
    c.clear(grey);
    long_run.render(c);
    int left = 0, right = 0, top = 0, bottom = 0;
    for (int y = 0; y < 240; ++y) {
        for (int x = 0; x < 320; ++x) {
            if (c.pixels()[y * 320 + x] == clear.pixels()[0]) continue;
            (x < 160 ? left : right)++;
            (y < 120 ? top : bottom)++;
        }
    }
    CHECK(left > 200 && right > 200 && top > 200 && bottom > 200);
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
    test_weather_vection();
    test_lanes();
    test_start_line();
    test_vehicles();
    test_car_models();
    test_track_map();
    test_gas_stations();
    test_wet_spots();
    test_san_francisco();
    test_branches();
    test_nitro();
    test_speed_rules();
    test_yield_lane();
    test_follow_speed();
    test_steer_rate();
    test_pause_menu();
    test_dirt();
    test_lots();
    test_reverse();
    test_fuel();
    test_crash_pose();
    test_vertical();
    test_road_mirror();
    test_framebuffer_blit();
    test_road_width();
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
