// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <array>
#include <string>

namespace racer {

// What keys and pad buttons can be bound to (CONTROLS in the pause menu).
// Not bindable, always there: Esc (back / quit), F8, F11 and Alt+Enter on
// the keyboard; Start (pause, the menu) on a pad; the arrow keys, Enter
// and the D-pad, A and B in the menus.
enum class Action : int {
    Accelerate,
    Brake,
    SteerLeft,
    SteerRight,
    Nitro,
    Handbrake,
    Horn,
    Headlights,
    IndicatorLeft,
    IndicatorRight,
    Hazards,
    Camera,
    Map,
    RadioNext,
    RadioPrevious,
    Restart,
    Pause,
    Mute,
    count
};
constexpr int action_count = static_cast<int>(Action::count);
constexpr int binding_slots = 2; // two keys (or pad inputs) per action

const char* action_name(Action a); // as the menu shows it: "STEER LEFT"
const char* action_id(Action a);   // as the choices file has it: "steer_left"

// A pad input: a button of SDL's standard controller layout, or an axis
// pushed one way (the triggers, the sticks). 0 is none.
constexpr int pad_none = 0;
constexpr int pad_axis_base = 64;
constexpr int pad_inputs = pad_axis_base + 2 * 6; // SDL has 6 controller axes
constexpr int pad_button(int sdl_button) { return 1 + sdl_button; }
constexpr int pad_axis(int sdl_axis, bool positive) { return pad_axis_base + 2 * sdl_axis + (positive ? 1 : 0); }
constexpr bool pad_is_axis(int p) { return p >= pad_axis_base && p < pad_inputs; }
constexpr bool pad_is_button(int p) { return p >= 1 && p < pad_axis_base; }
constexpr int pad_button_of(int p) { return p - 1; }
constexpr int pad_axis_of(int p) { return (p - pad_axis_base) / 2; }
constexpr bool pad_positive(int p) { return (p - pad_axis_base) % 2 == 1; }

// Short upper-case names for the menu: "LEFT CTRL", "KP ENTER", "LT",
// "R STICK UP"; "-" for none.
std::string key_label(int scancode);
std::string pad_label(int pad);

struct Bindings {
    // SDL scancodes (layout-independent, stable between SDL versions), 0 none.
    std::array<std::array<int, binding_slots>, action_count> keys{};
    std::array<std::array<int, binding_slots>, action_count> pad{};

    bool has_key(Action a, int scancode) const;
    bool has_pad(Action a, int pad_input) const;
};
Bindings default_bindings();

// Binds `code` to the action's slot. Whatever had it before loses it, so a
// key or button always does one thing. 0 clears the slot.
void bind_key(Bindings& b, Action a, int slot, int scancode);
void bind_pad(Bindings& b, Action a, int slot, int pad_input);

} // namespace racer
