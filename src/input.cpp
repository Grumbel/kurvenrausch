#include "input.hpp"

#include <SDL2/SDL.h>

namespace racer {

void Input::poll(InputState& state) {
    state.quit = false;
    state.restart = false;
    state.toggle_fullscreen = false;

    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) {
            state.quit = true;
        } else if (e.type == SDL_KEYDOWN && !e.key.repeat) {
            switch (e.key.keysym.sym) {
                case SDLK_ESCAPE: state.quit = true; break;
                case SDLK_r: state.restart = true; break;
                case SDLK_F11: state.toggle_fullscreen = true; break;
                case SDLK_RETURN:
                    if (e.key.keysym.mod & KMOD_ALT) state.toggle_fullscreen = true;
                    break;
                default: break;
            }
        }
    }

    const Uint8* keys = SDL_GetKeyboardState(nullptr);
    state.up    = keys[SDL_SCANCODE_UP] || keys[SDL_SCANCODE_W];
    state.down  = keys[SDL_SCANCODE_DOWN] || keys[SDL_SCANCODE_S];
    state.left  = keys[SDL_SCANCODE_LEFT] || keys[SDL_SCANCODE_A];
    state.right = keys[SDL_SCANCODE_RIGHT] || keys[SDL_SCANCODE_D];
}

} // namespace racer
