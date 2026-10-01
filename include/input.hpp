// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <SDL2/SDL.h>
#include <vector>

namespace racer {

// What the player asks the car to do this tick. Digital inputs (keyboard,
// D-pad, buttons) give exactly 0 or 1, analog ones (sticks, triggers) the
// range in between.
struct InputState {
    float throttle = 0.f; // 0 .. 1
    float brake = 0.f;    // 0 .. 1
    float steer = 0.f;    // -1 (left) .. +1 (right)

    // One-shot events since the last poll.
    bool quit = false;
    bool restart = false;
    bool toggle_fullscreen = false;
    bool toggle_mute = false;
};

// A normalised reading of one game controller (SDL's standard layout, which
// SDL maps Xbox, PlayStation, Switch and most other pads onto).
struct PadState {
    float left_x = 0.f;         // left stick, -1 .. +1
    float trigger_left = 0.f;   // 0 .. 1
    float trigger_right = 0.f;  // 0 .. 1
    bool dpad_left = false;
    bool dpad_right = false;
    bool a = false;             // accelerate, as an alternative to the trigger
    bool b = false;             // brake, as an alternative to the trigger
};

// Zeroes |v| <= deadzone and rescales the rest to the full -1 .. +1 range, so
// there is no jump when leaving the dead zone.
float apply_deadzone(float v, float deadzone);

// Combines a controller reading into `in`: steering adds up (clamped), throttle
// and brake take the larger value, so keyboard and pads can be used together.
void merge_pad(InputState& in, const PadState& pad);

class Input {
public:
    Input() = default;
    ~Input();
    Input(const Input&) = delete;
    Input& operator=(const Input&) = delete;

    // Starts the joystick / game controller subsystem. Controllers that are
    // already plugged in are reported as events and opened by poll(); the ones
    // plugged in later are handled the same way.
    bool init();

    // Polls SDL events. Held inputs are refreshed, one-shot events are set when
    // they occurred since the last poll.
    void poll(InputState& state);

    // Force feedback on all connected controllers. Strengths are 0 .. 1.
    // Controllers without rumble support ignore this.
    void rumble(float strong, float weak, int milliseconds);

    int controller_count() const { return static_cast<int>(pads_.size()); }

private:
    void open_controller(int device_index);
    void close_controller(SDL_JoystickID instance);

    std::vector<SDL_GameController*> pads_;
};

} // namespace racer
