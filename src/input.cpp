// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "input.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>

namespace racer {

namespace {

constexpr float stick_deadzone = 0.15f;
constexpr float trigger_deadzone = 0.05f;

float axis(SDL_GameController* pad, SDL_GameControllerAxis which) {
    return std::clamp(static_cast<float>(SDL_GameControllerGetAxis(pad, which)) / 32767.f, -1.f, 1.f);
}

bool button(SDL_GameController* pad, SDL_GameControllerButton which) {
    return SDL_GameControllerGetButton(pad, which) != 0;
}

} // namespace

float apply_deadzone(float v, float deadzone) {
    const float a = std::abs(v);
    if (a <= deadzone) return 0.f;
    const float scaled = std::min(1.f, (a - deadzone) / (1.f - deadzone));
    return v < 0.f ? -scaled : scaled;
}

void merge_pad(InputState& in, const PadState& pad) {
    float steer = apply_deadzone(pad.left_x, stick_deadzone);
    if (pad.dpad_left) steer -= 1.f;
    if (pad.dpad_right) steer += 1.f;
    in.steer = std::clamp(in.steer + steer, -1.f, 1.f);

    const float throttle = std::max(apply_deadzone(pad.trigger_right, trigger_deadzone),
                                    pad.a ? 1.f : 0.f);
    const float brake = std::max(apply_deadzone(pad.trigger_left, trigger_deadzone),
                                 pad.b ? 1.f : 0.f);
    in.throttle = std::max(in.throttle, throttle);
    in.brake = std::max(in.brake, brake);
}

Input::~Input() {
    for (SDL_GameController* pad : pads_) SDL_GameControllerClose(pad);
    if (SDL_WasInit(SDL_INIT_GAMECONTROLLER)) SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);
}

bool Input::init() {
    // The game should stay drivable when the window loses focus for a moment.
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) != 0) {
        std::cerr << "Game controller support unavailable: " << SDL_GetError() << "\n";
        return false;
    }
    return true;
}

void Input::open_controller(int device_index) {
    if (!SDL_IsGameController(device_index)) return;
    const SDL_JoystickID instance = SDL_JoystickGetDeviceInstanceID(device_index);
    if (SDL_GameControllerFromInstanceID(instance)) return; // already open

    SDL_GameController* pad = SDL_GameControllerOpen(device_index);
    if (!pad) {
        std::cerr << "Opening controller failed: " << SDL_GetError() << "\n";
        return;
    }
    pads_.push_back(pad);
    const char* name = SDL_GameControllerName(pad);
    std::cout << "Controller connected: " << (name ? name : "unknown") << "\n";
}

void Input::close_controller(SDL_JoystickID instance) {
    for (auto it = pads_.begin(); it != pads_.end(); ++it) {
        if (SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(*it)) != instance) continue;
        std::cout << "Controller disconnected\n";
        SDL_GameControllerClose(*it);
        pads_.erase(it);
        return;
    }
}

void Input::poll(InputState& state) {
    state.quit = false;
    state.restart = false;
    state.toggle_fullscreen = false;

    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
            case SDL_QUIT:
                state.quit = true;
                break;
            case SDL_KEYDOWN:
                if (e.key.repeat) break;
                switch (e.key.keysym.sym) {
                    case SDLK_ESCAPE: state.quit = true; break;
                    case SDLK_r: state.restart = true; break;
                    case SDLK_F11: state.toggle_fullscreen = true; break;
                    case SDLK_RETURN:
                        if (e.key.keysym.mod & KMOD_ALT) state.toggle_fullscreen = true;
                        break;
                    default: break;
                }
                break;
            case SDL_CONTROLLERDEVICEADDED:
                open_controller(e.cdevice.which);
                break;
            case SDL_CONTROLLERDEVICEREMOVED:
                close_controller(e.cdevice.which);
                break;
            case SDL_CONTROLLERBUTTONDOWN:
                if (e.cbutton.button == SDL_CONTROLLER_BUTTON_START) state.restart = true;
                break;
            default:
                break;
        }
    }

    const Uint8* keys = SDL_GetKeyboardState(nullptr);
    const bool up = keys[SDL_SCANCODE_UP] || keys[SDL_SCANCODE_W];
    const bool down = keys[SDL_SCANCODE_DOWN] || keys[SDL_SCANCODE_S];
    const bool left = keys[SDL_SCANCODE_LEFT] || keys[SDL_SCANCODE_A];
    const bool right = keys[SDL_SCANCODE_RIGHT] || keys[SDL_SCANCODE_D];
    state.throttle = up ? 1.f : 0.f;
    state.brake = down ? 1.f : 0.f;
    state.steer = (right ? 1.f : 0.f) - (left ? 1.f : 0.f);

    for (SDL_GameController* pad : pads_) {
        PadState p;
        p.left_x = axis(pad, SDL_CONTROLLER_AXIS_LEFTX);
        p.trigger_left = axis(pad, SDL_CONTROLLER_AXIS_TRIGGERLEFT);
        p.trigger_right = axis(pad, SDL_CONTROLLER_AXIS_TRIGGERRIGHT);
        p.dpad_left = button(pad, SDL_CONTROLLER_BUTTON_DPAD_LEFT);
        p.dpad_right = button(pad, SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
        p.a = button(pad, SDL_CONTROLLER_BUTTON_A);
        p.b = button(pad, SDL_CONTROLLER_BUTTON_B);
        merge_pad(state, p);
    }
}

void Input::rumble(float strong, float weak, int milliseconds) {
#if SDL_VERSION_ATLEAST(2, 0, 9)
    const auto to16 = [](float v) {
        return static_cast<Uint16>(std::clamp(v, 0.f, 1.f) * 65535.f);
    };
    for (SDL_GameController* pad : pads_) {
        SDL_GameControllerRumble(pad, to16(strong), to16(weak), static_cast<Uint32>(milliseconds));
    }
#else
    (void)strong; (void)weak; (void)milliseconds;
#endif
}

} // namespace racer
