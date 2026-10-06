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
// An axis counts as pressed past this, released again under the second.
constexpr float axis_press = 0.6f;
constexpr float axis_release = 0.3f;
// Menu directions held on a pad repeat after a moment.
constexpr Uint32 repeat_delay_ms = 350;
constexpr Uint32 repeat_every_ms = 90;

bool is_trigger(int axis) {
    return axis == SDL_CONTROLLER_AXIS_TRIGGERLEFT || axis == SDL_CONTROLLER_AXIS_TRIGGERRIGHT;
}

// The same stick's other axis: a diagonal push counts only the stronger way.
int other_axis(int axis) {
    switch (axis) {
        case SDL_CONTROLLER_AXIS_LEFTX: return SDL_CONTROLLER_AXIS_LEFTY;
        case SDL_CONTROLLER_AXIS_LEFTY: return SDL_CONTROLLER_AXIS_LEFTX;
        case SDL_CONTROLLER_AXIS_RIGHTX: return SDL_CONTROLLER_AXIS_RIGHTY;
        case SDL_CONTROLLER_AXIS_RIGHTY: return SDL_CONTROLLER_AXIS_RIGHTX;
        default: return -1;
    }
}

void set_dir(MenuInput& m, int dir) {
    if (dir == 1) m.up = true;
    if (dir == 2) m.down = true;
    if (dir == 3) m.left = true;
    if (dir == 4) m.right = true;
}

} // namespace

float apply_deadzone(float v, float deadzone) {
    const float a = std::abs(v);
    if (a <= deadzone) return 0.f;
    const float scaled = std::min(1.f, (a - deadzone) / (1.f - deadzone));
    return v < 0.f ? -scaled : scaled;
}

float pad_value(const PadState& pad, int p) {
    if (pad_is_button(p)) {
        const int b = pad_button_of(p);
        return b < static_cast<int>(pad.buttons.size()) && pad.buttons[static_cast<size_t>(b)] ? 1.f : 0.f;
    }
    if (!pad_is_axis(p)) return 0.f;
    const int axis = pad_axis_of(p);
    const float v = pad.axes[static_cast<size_t>(axis)] * (pad_positive(p) ? 1.f : -1.f);
    return std::max(0.f, apply_deadzone(v, is_trigger(axis) ? trigger_deadzone : stick_deadzone));
}

void merge_pad(InputState& in, const PadState& pad, const Bindings& bindings) {
    const auto value = [&](Action a) {
        float v = 0.f;
        for (int p : bindings.pad[static_cast<size_t>(a)]) v = std::max(v, pad_value(pad, p));
        return v;
    };
    in.steer = std::clamp(in.steer + value(Action::SteerRight) - value(Action::SteerLeft), -1.f, 1.f);
    in.throttle = std::max(in.throttle, value(Action::Accelerate));
    in.brake = std::max(in.brake, value(Action::Brake));
    in.horn = in.horn || value(Action::Horn) > 0.5f;
    in.handbrake = in.handbrake || value(Action::Handbrake) > 0.5f;
    in.nitro = in.nitro || value(Action::Nitro) > 0.5f;
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

PadState Input::read(SDL_GameController* pad) const {
    PadState p;
    for (int a = 0; a < SDL_CONTROLLER_AXIS_MAX && a < static_cast<int>(p.axes.size()); ++a) {
        const float v = static_cast<float>(SDL_GameControllerGetAxis(pad, static_cast<SDL_GameControllerAxis>(a)));
        p.axes[static_cast<size_t>(a)] = std::clamp(v / 32767.f, -1.f, 1.f);
    }
    for (int b = 0; b < SDL_CONTROLLER_BUTTON_MAX && b < static_cast<int>(p.buttons.size()); ++b)
        p.buttons[static_cast<size_t>(b)] =
            SDL_GameControllerGetButton(pad, static_cast<SDL_GameControllerButton>(b)) != 0;
    return p;
}

void Input::fire(InputState& state, Action a, bool keyboard) const {
    switch (a) {
        case Action::Pause: state.pause = true; break;
        case Action::Restart: state.restart = true; break;
        case Action::Camera: state.change_view = true; break;
        case Action::Map: state.toggle_map = true; break;
        case Action::Headlights: state.toggle_headlights = true; break;
        case Action::IndicatorLeft: state.signal_left = true; break;
        case Action::IndicatorRight: state.signal_right = true; break;
        case Action::Hazards: state.toggle_hazards = true; break;
        case Action::RadioNext: state.change_music = 1; break;
        case Action::RadioPrevious: state.change_music = -1; break;
        case Action::Mute: state.toggle_mute = true; break;
        // The driving keys move through the menus too (WASD as the arrows);
        // on a pad the D-pad and the left stick do, whatever is bound.
        case Action::Accelerate: if (keyboard) state.menu.up = true; break;
        case Action::Brake: if (keyboard) state.menu.down = true; break;
        case Action::SteerLeft: if (keyboard) state.menu.left = true; break;
        case Action::SteerRight: if (keyboard) state.menu.right = true; break;
        default: break;
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
    state.toggle_renderer = false;
    state.toggle_map = false;
    state.toggle_headlights = state.signal_left = state.signal_right = state.toggle_hazards = false;
    state.change_music = 0;
    state.taps.clear();
    state.clicks.clear();
    state.pointer_moved = false;
    state.captured_key = 0;
    state.captured_pad = 0;
    state.any_input = false;

    const auto window_xy = [](Uint32 window_id, int x, int y, float& nx, float& ny) {
        int w = 0, h = 0;
        if (SDL_Window* win = SDL_GetWindowFromID(window_id)) SDL_GetWindowSize(win, &w, &h);
        nx = w > 0 ? static_cast<float>(x) / static_cast<float>(w) : 0.f;
        ny = h > 0 ? static_cast<float>(y) / static_cast<float>(h) : 0.f;
    };

    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        switch (e.type) {
            case SDL_QUIT:
                state.quit = true;
                break;
            case SDL_KEYDOWN: {
                const SDL_Scancode sc = e.key.keysym.scancode;
                const SDL_Keycode sym = e.key.keysym.sym;
                if (e.key.repeat) {
                    // Held, the menu's selection keeps moving; nothing else repeats.
                    if (capture_) break;
                    if (sym == SDLK_UP || bindings_.has_key(Action::Accelerate, sc)) state.menu.up = true;
                    if (sym == SDLK_DOWN || bindings_.has_key(Action::Brake, sc)) state.menu.down = true;
                    if (sym == SDLK_LEFT || bindings_.has_key(Action::SteerLeft, sc)) state.menu.left = true;
                    if (sym == SDLK_RIGHT || bindings_.has_key(Action::SteerRight, sc)) state.menu.right = true;
                    if (sym == SDLK_PAGEUP) state.menu.page_up = true;
                    if (sym == SDLK_PAGEDOWN) state.menu.page_down = true;
                    break;
                }
                state.any_input = true;
                if (capture_) {
                    if (sym == SDLK_ESCAPE || sym == SDLK_AC_BACK) state.menu.back = true;
                    else if (sym != SDLK_F8 && sym != SDLK_F11 && sc != SDL_SCANCODE_UNKNOWN) state.captured_key = sc;
                    break;
                }
                // Fixed keys, whatever is bound.
                switch (sym) {
                    case SDLK_ESCAPE: state.escape = true; state.menu.back = true; break;
                    case SDLK_AC_BACK: // Android's back button: pause, or back out of the menu
                        state.pause = true;
                        state.menu.back = true;
                        break;
                    case SDLK_BACKSPACE: state.menu.back = true; break;
                    case SDLK_DELETE: state.menu.clear = true; break;
                    case SDLK_UP: state.menu.up = true; break;
                    case SDLK_DOWN: state.menu.down = true; break;
                    case SDLK_LEFT: state.menu.left = true; break;
                    case SDLK_RIGHT: state.menu.right = true; break;
                    case SDLK_PAGEUP: state.menu.page_up = true; break;
                    case SDLK_PAGEDOWN: state.menu.page_down = true; break;
                    case SDLK_SPACE: state.menu.confirm = true; break;
                    case SDLK_F11: state.toggle_fullscreen = true; break;
                    case SDLK_F8: state.toggle_renderer = true; break;
                    case SDLK_RETURN:
                    case SDLK_KP_ENTER:
                        if (e.key.keysym.mod & KMOD_ALT) {
                            state.toggle_fullscreen = true;
                            continue; // not also what Enter is bound to
                        }
                        state.menu.confirm = true;
                        break;
                    default: break;
                }
                for (int a = 0; a < action_count; ++a)
                    if (bindings_.has_key(static_cast<Action>(a), sc)) fire(state, static_cast<Action>(a), true);
                break;
            }
            case SDL_FINGERDOWN:
                state.any_input = true;
                fingers_[e.tfinger.fingerId] = Finger{static_cast<int64_t>(e.tfinger.fingerId), e.tfinger.x, e.tfinger.y};
                state.taps.push_back(fingers_[e.tfinger.fingerId]);
                break;
            case SDL_FINGERMOTION:
                fingers_[e.tfinger.fingerId] = Finger{static_cast<int64_t>(e.tfinger.fingerId), e.tfinger.x, e.tfinger.y};
                break;
            case SDL_FINGERUP:
                fingers_.erase(e.tfinger.fingerId);
                break;
            // The mouse; what SDL makes up from touches comes as fingers already.
            case SDL_MOUSEMOTION:
                if (e.motion.which == SDL_TOUCH_MOUSEID) break;
                state.pointer_moved = true;
                window_xy(e.motion.windowID, e.motion.x, e.motion.y, state.pointer_x, state.pointer_y);
                break;
            case SDL_MOUSEBUTTONDOWN:
                if (e.button.which == SDL_TOUCH_MOUSEID) break;
                state.any_input = true;
                if (capture_ || e.button.button == SDL_BUTTON_RIGHT) {
                    state.menu.back = true;
                } else if (e.button.button == SDL_BUTTON_LEFT) {
                    Finger click{-1, 0.f, 0.f};
                    window_xy(e.button.windowID, e.button.x, e.button.y, click.x, click.y);
                    state.clicks.push_back(click);
                }
                break;
            case SDL_MOUSEWHEEL: {
                if (e.wheel.which == SDL_TOUCH_MOUSEID) break;
                const int y = e.wheel.direction == SDL_MOUSEWHEEL_FLIPPED ? -e.wheel.y : e.wheel.y;
                state.menu.scroll -= y; // wheel up: up the list
                break;
            }
            case SDL_CONTROLLERDEVICEADDED:
                open_controller(e.cdevice.which);
                break;
            case SDL_CONTROLLERDEVICEREMOVED:
                close_controller(e.cdevice.which);
                break;
            case SDL_CONTROLLERBUTTONDOWN: {
                state.any_input = true;
                const int button = e.cbutton.button;
                if (capture_) {
                    if (button == SDL_CONTROLLER_BUTTON_START) state.menu.back = true;
                    else state.captured_pad = pad_button(button);
                    break;
                }
                // Fixed: Start pauses; the menu's buttons.
                switch (button) {
                    case SDL_CONTROLLER_BUTTON_START: state.pause = true; break;
                    case SDL_CONTROLLER_BUTTON_DPAD_UP: state.menu.up = true; break;
                    case SDL_CONTROLLER_BUTTON_DPAD_DOWN: state.menu.down = true; break;
                    case SDL_CONTROLLER_BUTTON_DPAD_LEFT: state.menu.left = true; break;
                    case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: state.menu.right = true; break;
                    case SDL_CONTROLLER_BUTTON_A: state.menu.confirm = true; break;
                    case SDL_CONTROLLER_BUTTON_B: state.menu.back = true; break;
                    case SDL_CONTROLLER_BUTTON_X: state.menu.clear = true; break;
                    case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: state.menu.page_up = true; break;
                    case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: state.menu.page_down = true; break;
                    default: break;
                }
                if (button == SDL_CONTROLLER_BUTTON_START) break;
                for (int a = 0; a < action_count; ++a)
                    if (bindings_.has_pad(static_cast<Action>(a), pad_button(button)))
                        fire(state, static_cast<Action>(a), false);
                break;
            }
            default:
                break;
        }
    }

    // The keyboard, held.
    const Uint8* keys = SDL_GetKeyboardState(nullptr);
    const auto held = [&](Action a) {
        for (int k : bindings_.keys[static_cast<size_t>(a)])
            if (k > 0 && k < SDL_NUM_SCANCODES && keys[k]) return true;
        return false;
    };
    state.throttle = held(Action::Accelerate) ? 1.f : 0.f;
    state.brake = held(Action::Brake) ? 1.f : 0.f;
    state.steer = (held(Action::SteerRight) ? 1.f : 0.f) - (held(Action::SteerLeft) ? 1.f : 0.f);
    state.horn = held(Action::Horn);
    state.nitro = held(Action::Nitro);
    state.handbrake = held(Action::Handbrake);
    state.fingers.clear();
    for (const auto& [id, finger] : fingers_) state.fingers.push_back(finger);

    std::vector<PadState> pads;
    for (SDL_GameController* pad : pads_) pads.push_back(read(pad));
    for (const PadState& p : pads) merge_pad(state, p, bindings_);

    // The axes pushed each way: the strongest push of any pad, each axis.
    // A push past axis_press counts once (as a button press would), until
    // the axis comes back under axis_release; a diagonal push of a stick
    // counts only its stronger way.
    std::array<float, 6> axes{};
    for (const PadState& p : pads)
        for (size_t a = 0; a < axes.size(); ++a)
            if (std::abs(p.axes[a]) > std::abs(axes[a])) axes[a] = p.axes[a];
    int stick_dir = 0; // the left stick in the menu: 1 up, 2 down, 3 left, 4 right
    for (int a = 0; a < static_cast<int>(axes.size()); ++a) {
        for (const bool positive : {false, true}) {
            const int p = pad_axis(a, positive);
            const float v = axes[static_cast<size_t>(a)] * (positive ? 1.f : -1.f);
            const int other = other_axis(a);
            const bool stronger = other < 0 || std::abs(axes[static_cast<size_t>(a)]) >= std::abs(axes[static_cast<size_t>(other)]);
            bool& down = axis_down_[static_cast<size_t>(p)];
            const bool pressed = v > axis_press && stronger;
            if (pressed && a == SDL_CONTROLLER_AXIS_LEFTX) stick_dir = positive ? 4 : 3;
            if (pressed && a == SDL_CONTROLLER_AXIS_LEFTY) stick_dir = positive ? 2 : 1;
            if (pressed && !down) {
                state.any_input = true;
                if (capture_) {
                    if (state.captured_pad == pad_none) state.captured_pad = p;
                } else {
                    if (a == SDL_CONTROLLER_AXIS_LEFTX || a == SDL_CONTROLLER_AXIS_LEFTY) set_dir(state.menu, stick_dir);
                    for (int act = 0; act < action_count; ++act)
                        if (bindings_.has_pad(static_cast<Action>(act), p)) fire(state, static_cast<Action>(act), false);
                }
            }
            if (pressed) down = true;
            else if (v < axis_release) down = false;
        }
    }

    // A D-pad direction or the left stick held repeats in the menu.
    int held_dir = stick_dir;
    for (const PadState& p : pads) {
        if (p.buttons[SDL_CONTROLLER_BUTTON_DPAD_UP]) held_dir = 1;
        if (p.buttons[SDL_CONTROLLER_BUTTON_DPAD_DOWN]) held_dir = 2;
        if (p.buttons[SDL_CONTROLLER_BUTTON_DPAD_LEFT]) held_dir = 3;
        if (p.buttons[SDL_CONTROLLER_BUTTON_DPAD_RIGHT]) held_dir = 4;
    }
    const Uint32 now = SDL_GetTicks();
    if (held_dir != repeat_dir_) {
        repeat_dir_ = held_dir;
        repeat_at_ = now + repeat_delay_ms;
    } else if (held_dir != 0 && !capture_ && static_cast<Sint32>(now - repeat_at_) >= 0) {
        set_dir(state.menu, held_dir);
        repeat_at_ = now + repeat_every_ms;
    }

    // Sticks and triggers pushed well over count too, as do fingers held on
    // the screen.
    const float right_x = axes[SDL_CONTROLLER_AXIS_RIGHTX], right_y = axes[SDL_CONTROLLER_AXIS_RIGHTY];
    if (std::abs(state.steer) > 0.5f || state.throttle > 0.5f || state.brake > 0.5f || std::abs(right_x) > 0.6f ||
        std::abs(right_y) > 0.6f || !state.fingers.empty()) {
        state.any_input = true;
    }

    // Anything held at all.
    int key_count = 0;
    keys = SDL_GetKeyboardState(&key_count);
    state.any_held = !state.fingers.empty() || SDL_GetMouseState(nullptr, nullptr) != 0;
    for (int k = 0; k < key_count && !state.any_held; ++k) state.any_held = keys[k] != 0;
    for (const PadState& p : pads) {
        for (bool b : p.buttons) state.any_held = state.any_held || b;
        for (float a : p.axes) state.any_held = state.any_held || std::abs(a) > axis_release;
    }
}

void Input::rumble(float strong, float weak, int milliseconds) {
    if (!rumble_on_) return;
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
