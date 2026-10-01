// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

// Minimal self-contained unit tests; no framework needed. Run with `ctest`
// or directly: ./kurvenrausch_tests

#include "input.hpp"

#include <cmath>
#include <cstdio>

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

} // namespace

int main() {
    test_deadzone();
    test_pad_mapping();

    if (failures) {
        std::fprintf(stderr, "%d check(s) failed\n", failures);
        return 1;
    }
    std::puts("all tests passed");
    return 0;
}
