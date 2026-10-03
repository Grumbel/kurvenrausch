// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "menu.hpp"
#include "touch.hpp"

#include <SDL2/SDL.h>
#include <map>
#include <vector>

namespace racer {

// What the player asks the car to do this tick. Digital inputs (keyboard,
// D-pad, buttons) give exactly 0 or 1, analog ones (sticks, triggers) the
// range in between.
struct InputState {
    float throttle = 0.f; // 0 .. 1
    float brake = 0.f;    // 0 .. 1
    float steer = 0.f;    // -1 (left) .. +1 (right)
    bool horn = false;    // held
    bool nitro = false;   // held; a burn starts when it is pressed
    bool handbrake = false; // held

    // One-shot events since the last poll.
    bool quit = false;    // the window was closed
    bool escape = false;  // Esc: quits while driving, leaves the pause menu
    bool pause = false;   // P or Start: opens and closes the pause menu
    bool restart = false;
    MenuInput menu;       // navigating the pause menu
    bool toggle_fullscreen = false;
    bool toggle_mute = false;
    bool change_view = false; // C or Back: the next camera view
    int change_music = 0;     // N, or the right stick flicked: +1 the next track, -1 the previous

    // The touch screen, in window coordinates from 0 to 1: the fingers down
    // now, and those that came down since the last poll.
    std::vector<Finger> fingers;
    std::vector<Finger> taps;
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
    bool x = false;             // horn
    bool y = false;             // nitro
    bool left_shoulder = false; // handbrake
    bool right_shoulder = false;// nitro
};

// Zeroes |v| <= deadzone and rescales the rest to the full -1 .. +1 range, so
// there is no jump when leaving the dead zone.
float apply_deadzone(float v, float deadzone);

// Combines a controller reading into `in`: steering adds up (clamped), throttle
// and brake take the larger value, buttons are or-ed, so keyboard and pads can
// be used together.
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
    std::map<SDL_FingerID, Finger> fingers_; // down now, window coordinates 0 .. 1
    int music_stick_ = 0; // where the right stick was flicked last: -1, 0 or +1
};

} // namespace racer
