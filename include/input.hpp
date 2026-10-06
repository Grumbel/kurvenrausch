// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "bindings.hpp"
#include "menu.hpp"
#include "touch.hpp"

#include <SDL2/SDL.h>
#include <array>
#include <map>
#include <vector>

namespace racer {

// What the player asks the car to do this tick. Digital inputs (keyboard,
// D-pad, buttons) give exactly 0 or 1, analog ones (sticks, triggers) the
// range in between. Which keys and buttons do what is up to the Bindings.
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
    MenuInput menu;       // navigating the pause menu (arrows, Enter, Esc, D-pad, A, B, ...)
    bool toggle_fullscreen = false;
    bool toggle_mute = false;
    bool change_view = false; // C or Back: the next camera view
    bool toggle_renderer = false; // F8: cycle software / GLES scene renderer
    bool toggle_map = false;  // Tab or the right stick clicked: zoom the mini map in or out
    // The light switches: headlights, indicators, hazard lights. (In the
    // pause menu the D-pad moves the selection instead.)
    bool toggle_headlights = false;
    bool signal_left = false;
    bool signal_right = false;
    bool toggle_hazards = false;
    int change_music = 0;     // +1 the next radio track, -1 the previous

    // The mouse, in window coordinates from 0 to 1 (as the fingers): where it
    // went, if it moved, and where the left button was clicked. A right click
    // is menu.back.
    bool pointer_moved = false;
    float pointer_x = 0.f, pointer_y = 0.f;
    std::vector<Finger> clicks;

    // Rebinding (Input::set_capture): the key or pad input pressed, 0 none.
    // Esc, Start or a click cancel instead (menu.back).
    int captured_key = 0;
    int captured_pad = 0;

    // The touch screen, in window coordinates from 0 to 1: the fingers down
    // now, and those that came down since the last poll.
    std::vector<Finger> fingers;
    std::vector<Finger> taps;

    // Anything at all since the last poll: a key or button pressed, the
    // screen touched, a stick or trigger pushed (ends the attract mode).
    bool any_input = false;
    // Anything held down right now: a key, a button, a stick or trigger
    // pushed, a finger on the screen, a mouse button.
    bool any_held = false;
};

// A reading of one game controller in SDL's standard layout (which SDL maps
// Xbox, PlayStation, Switch and most other pads onto): its axes (sticks
// -1 .. +1, triggers 0 .. 1) and buttons, indexed by SDL's enums.
struct PadState {
    std::array<float, 6> axes{};
    std::array<bool, 32> buttons{};
};

// Zeroes |v| <= deadzone and rescales the rest to the full -1 .. +1 range, so
// there is no jump when leaving the dead zone.
float apply_deadzone(float v, float deadzone);

// How far a pad input is pressed, 0 .. 1: buttons 0 or 1, triggers and
// sticks pushed its way with their dead zones.
float pad_value(const PadState& pad, int pad_input);

// Combines a controller reading into `in` as `bindings` say: steering adds up
// (clamped), throttle and brake take the larger value, buttons are or-ed, so
// keyboard and pads can be used together.
void merge_pad(InputState& in, const PadState& pad, const Bindings& bindings);

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

    void set_bindings(const Bindings& b) { bindings_ = b; }
    const Bindings& bindings() const { return bindings_; }
    // While capturing, the next key or pad input pressed is reported in
    // captured_key / captured_pad and does nothing else.
    void set_capture(bool on) { capture_ = on; }
    void set_rumble(bool on) { rumble_on_ = on; }

private:
    // What a bound key or pad input pressed (not held) does.
    void fire(InputState& state, Action a, bool keyboard) const;
    PadState read(SDL_GameController* pad) const;

    void open_controller(int device_index);
    void close_controller(SDL_JoystickID instance);

    std::vector<SDL_GameController*> pads_;
    std::map<SDL_FingerID, Finger> fingers_; // down now, window coordinates 0 .. 1

    Bindings bindings_ = default_bindings();
    bool capture_ = false;
    bool rumble_on_ = true;
    // The axes pushed each way (pad inputs from pad_axis_base on): a push
    // counts once the axis has come back towards the middle.
    std::array<bool, pad_inputs> axis_down_{};
    // A D-pad direction or the left stick held in the menu repeats.
    int repeat_dir_ = 0;
    Uint32 repeat_at_ = 0;
};

} // namespace racer
