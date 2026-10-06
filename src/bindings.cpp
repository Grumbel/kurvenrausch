// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "bindings.hpp"

#include <SDL2/SDL.h>

#include <cctype>

namespace racer {

namespace {

struct ActionInfo {
    const char* name;
    const char* id;
};

constexpr ActionInfo actions[action_count] = {
    {"ACCELERATE", "accelerate"},
    {"BRAKE / REVERSE", "brake"},
    {"STEER LEFT", "steer_left"},
    {"STEER RIGHT", "steer_right"},
    {"NITRO", "nitro"},
    {"HANDBRAKE", "handbrake"},
    {"HORN", "horn"},
    {"HEADLIGHTS", "headlights"},
    {"INDICATOR LEFT", "indicator_left"},
    {"INDICATOR RIGHT", "indicator_right"},
    {"HAZARD LIGHTS", "hazards"},
    {"CAMERA VIEW", "camera"},
    {"MAP ZOOM", "map"},
    {"RADIO NEXT", "radio_next"},
    {"RADIO PREVIOUS", "radio_previous"},
    {"RESTART", "restart"},
    {"PAUSE", "pause"},
    {"MUTE", "mute"},
};

std::string upper(std::string s) {
    for (char& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return s;
}

void replace_prefix(std::string& s, const std::string& from, const std::string& to) {
    if (s.compare(0, from.size(), from) == 0) s = to + s.substr(from.size());
}

size_t index(Action a) { return static_cast<size_t>(a); }

} // namespace

const char* action_name(Action a) { return actions[index(a)].name; }
const char* action_id(Action a) { return actions[index(a)].id; }

std::string key_label(int scancode) {
    if (scancode <= 0 || scancode >= SDL_NUM_SCANCODES) return "-";
    std::string name = upper(SDL_GetScancodeName(static_cast<SDL_Scancode>(scancode)));
    if (name.empty()) return "KEY " + std::to_string(scancode);
    replace_prefix(name, "KEYPAD ", "KP ");
    if (name == "RETURN") name = "ENTER";
    if (name == "BACKSPACE") name = "BACKSP";
    return name;
}

std::string pad_label(int p) {
    if (pad_is_button(p)) {
        static const char* const names[] = {"A",     "B",       "X",        "Y",       "BACK",     "GUIDE",
                                            "START", "L STICK", "R STICK",  "LB",      "RB",       "D-PAD UP",
                                            "D-PAD DOWN", "D-PAD LEFT", "D-PAD RIGHT", "MISC", "PADDLE 1", "PADDLE 2",
                                            "PADDLE 3", "PADDLE 4", "TOUCHPAD"};
        const int b = pad_button_of(p);
        if (b >= 0 && b < static_cast<int>(sizeof names / sizeof names[0])) return names[b];
        return "BUTTON " + std::to_string(b);
    }
    if (pad_is_axis(p)) {
        const bool pos = pad_positive(p);
        switch (pad_axis_of(p)) {
            case SDL_CONTROLLER_AXIS_LEFTX: return pos ? "L STICK RIGHT" : "L STICK LEFT";
            case SDL_CONTROLLER_AXIS_LEFTY: return pos ? "L STICK DOWN" : "L STICK UP";
            case SDL_CONTROLLER_AXIS_RIGHTX: return pos ? "R STICK RIGHT" : "R STICK LEFT";
            case SDL_CONTROLLER_AXIS_RIGHTY: return pos ? "R STICK DOWN" : "R STICK UP";
            case SDL_CONTROLLER_AXIS_TRIGGERLEFT: return "LT";
            case SDL_CONTROLLER_AXIS_TRIGGERRIGHT: return "RT";
            default: break;
        }
    }
    return "-";
}

bool Bindings::has_key(Action a, int scancode) const {
    if (scancode == 0) return false;
    for (int k : keys[index(a)])
        if (k == scancode) return true;
    return false;
}

bool Bindings::has_pad(Action a, int pad_input) const {
    if (pad_input == pad_none) return false;
    for (int p : pad[index(a)])
        if (p == pad_input) return true;
    return false;
}

Bindings default_bindings() {
    Bindings b;
    const auto key = [&](Action a, int k0, int k1 = 0) { b.keys[index(a)] = {k0, k1}; };
    key(Action::Accelerate, SDL_SCANCODE_UP, SDL_SCANCODE_W);
    key(Action::Brake, SDL_SCANCODE_DOWN, SDL_SCANCODE_S);
    key(Action::SteerLeft, SDL_SCANCODE_LEFT, SDL_SCANCODE_A);
    key(Action::SteerRight, SDL_SCANCODE_RIGHT, SDL_SCANCODE_D);
    key(Action::Nitro, SDL_SCANCODE_SPACE);
    key(Action::Handbrake, SDL_SCANCODE_LCTRL, SDL_SCANCODE_RCTRL);
    key(Action::Horn, SDL_SCANCODE_H);
    key(Action::Headlights, SDL_SCANCODE_L);
    key(Action::IndicatorLeft, SDL_SCANCODE_Q);
    key(Action::IndicatorRight, SDL_SCANCODE_E);
    key(Action::Hazards, SDL_SCANCODE_Z);
    key(Action::Camera, SDL_SCANCODE_C);
    key(Action::Map, SDL_SCANCODE_TAB);
    key(Action::RadioNext, SDL_SCANCODE_N);
    key(Action::RadioPrevious, SDL_SCANCODE_B);
    key(Action::Restart, SDL_SCANCODE_R);
    key(Action::Pause, SDL_SCANCODE_P);
    key(Action::Mute, SDL_SCANCODE_M);

    const auto pad = [&](Action a, int p0, int p1 = pad_none) { b.pad[index(a)] = {p0, p1}; };
    pad(Action::Accelerate, pad_axis(SDL_CONTROLLER_AXIS_TRIGGERRIGHT, true), pad_button(SDL_CONTROLLER_BUTTON_A));
    pad(Action::Brake, pad_axis(SDL_CONTROLLER_AXIS_TRIGGERLEFT, true), pad_button(SDL_CONTROLLER_BUTTON_B));
    pad(Action::SteerLeft, pad_axis(SDL_CONTROLLER_AXIS_LEFTX, false), pad_button(SDL_CONTROLLER_BUTTON_DPAD_LEFT));
    pad(Action::SteerRight, pad_axis(SDL_CONTROLLER_AXIS_LEFTX, true), pad_button(SDL_CONTROLLER_BUTTON_DPAD_RIGHT));
    pad(Action::Nitro, pad_button(SDL_CONTROLLER_BUTTON_Y), pad_button(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER));
    pad(Action::Handbrake, pad_button(SDL_CONTROLLER_BUTTON_LEFTSHOULDER));
    pad(Action::Horn, pad_button(SDL_CONTROLLER_BUTTON_X));
    pad(Action::Headlights, pad_button(SDL_CONTROLLER_BUTTON_DPAD_UP));
    pad(Action::IndicatorLeft, pad_axis(SDL_CONTROLLER_AXIS_RIGHTX, false));
    pad(Action::IndicatorRight, pad_axis(SDL_CONTROLLER_AXIS_RIGHTX, true));
    pad(Action::Hazards, pad_button(SDL_CONTROLLER_BUTTON_DPAD_DOWN));
    pad(Action::Camera, pad_button(SDL_CONTROLLER_BUTTON_BACK));
    pad(Action::Map, pad_button(SDL_CONTROLLER_BUTTON_RIGHTSTICK));
    pad(Action::RadioNext, pad_axis(SDL_CONTROLLER_AXIS_RIGHTY, false));
    pad(Action::RadioPrevious, pad_axis(SDL_CONTROLLER_AXIS_RIGHTY, true));
    // Pause: Start, always (not bindable, see Action).
    return b;
}

void bind_key(Bindings& b, Action a, int slot, int scancode) {
    if (slot < 0 || slot >= binding_slots) return;
    if (scancode != 0) {
        for (auto& slots : b.keys)
            for (int& k : slots)
                if (k == scancode) k = 0;
    }
    b.keys[index(a)][static_cast<size_t>(slot)] = scancode;
}

void bind_pad(Bindings& b, Action a, int slot, int pad_input) {
    if (slot < 0 || slot >= binding_slots) return;
    if (pad_input != pad_none) {
        for (auto& slots : b.pad)
            for (int& p : slots)
                if (p == pad_input) p = pad_none;
    }
    b.pad[index(a)][static_cast<size_t>(slot)] = pad_input;
}

} // namespace racer
