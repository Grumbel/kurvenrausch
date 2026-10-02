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
    in.horn = in.horn || pad.x;
    in.handbrake = in.handbrake || pad.left_shoulder;
    in.nitro = in.nitro || pad.y || pad.right_shoulder;
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
    state.escape = false;
    state.pause = false;
    state.restart = false;
    state.menu = MenuInput{};
    state.toggle_fullscreen = false;
    state.toggle_mute = false;
    state.change_view = false;
    state.change_music = 0;

    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
            case SDL_QUIT:
                state.quit = true;
                break;
            case SDL_KEYDOWN:
                if (e.key.repeat) break;
                switch (e.key.keysym.sym) {
                    case SDLK_ESCAPE: state.escape = true; state.menu.back = true; break;
                    case SDLK_p: state.pause = true; break;
                    case SDLK_r: state.restart = true; break;
                    case SDLK_UP: case SDLK_w: state.menu.up = true; break;
                    case SDLK_DOWN: case SDLK_s: state.menu.down = true; break;
                    case SDLK_LEFT: case SDLK_a: state.menu.left = true; break;
                    case SDLK_RIGHT: case SDLK_d: state.menu.right = true; break;
                    case SDLK_SPACE: state.menu.confirm = true; break;
                    case SDLK_F11: state.toggle_fullscreen = true; break;
                    case SDLK_m: state.toggle_mute = true; break;
                    case SDLK_c: state.change_view = true; break;
                    case SDLK_n: state.change_music = 1; break;
                    case SDLK_RETURN:
                        if (e.key.keysym.mod & KMOD_ALT) state.toggle_fullscreen = true;
                        else state.menu.confirm = true;
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
                switch (e.cbutton.button) {
                    case SDL_CONTROLLER_BUTTON_START: state.pause = true; break;
                    case SDL_CONTROLLER_BUTTON_BACK: state.change_view = true; break;
                    case SDL_CONTROLLER_BUTTON_DPAD_UP: state.menu.up = true; break;
                    case SDL_CONTROLLER_BUTTON_DPAD_DOWN: state.menu.down = true; break;
                    case SDL_CONTROLLER_BUTTON_DPAD_LEFT: state.menu.left = true; break;
                    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: state.menu.right = true; break;
                    case SDL_CONTROLLER_BUTTON_A: state.menu.confirm = true; break;
                    case SDL_CONTROLLER_BUTTON_B: state.menu.back = true; break;
                    default: break;
                }
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
    state.horn = keys[SDL_SCANCODE_H];
    state.nitro = keys[SDL_SCANCODE_SPACE];
    state.handbrake = keys[SDL_SCANCODE_LCTRL] || keys[SDL_SCANCODE_RCTRL];

    // The right stick changes the radio's track: one step per flick, the
    // stick has to come back to the middle before the next.
    float right_x = 0.f;
    for (SDL_GameController* pad : pads_) {
        const float x = axis(pad, SDL_CONTROLLER_AXIS_RIGHTX);
        if (std::abs(x) > std::abs(right_x)) right_x = x;
    }
    const int flick = right_x > 0.6f ? 1 : right_x < -0.6f ? -1 : 0;
    if (flick != 0 && music_stick_ == 0) state.change_music = flick;
    if (flick != 0 || std::abs(right_x) < 0.3f) music_stick_ = flick;

    for (SDL_GameController* pad : pads_) {
        PadState p;
        p.left_x = axis(pad, SDL_CONTROLLER_AXIS_LEFTX);
        p.trigger_left = axis(pad, SDL_CONTROLLER_AXIS_TRIGGERLEFT);
        p.trigger_right = axis(pad, SDL_CONTROLLER_AXIS_TRIGGERRIGHT);
        p.dpad_left = button(pad, SDL_CONTROLLER_BUTTON_DPAD_LEFT);
        p.dpad_right = button(pad, SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
        p.a = button(pad, SDL_CONTROLLER_BUTTON_A);
        p.b = button(pad, SDL_CONTROLLER_BUTTON_B);
        p.x = button(pad, SDL_CONTROLLER_BUTTON_X);
        p.y = button(pad, SDL_CONTROLLER_BUTTON_Y);
        p.left_shoulder = button(pad, SDL_CONTROLLER_BUTTON_LEFTSHOULDER);
        p.right_shoulder = button(pad, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);
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
