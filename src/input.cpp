#include "input.hpp"

namespace racer {

void Input::poll(InputState& state) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) {
            state.quit = true;
        } else if (e.type == SDL_KEYDOWN) {
            switch (e.key.keysym.sym) {
                case SDLK_ESCAPE: state.quit = true; break;
                case SDLK_r: state.restart = true; break;
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
