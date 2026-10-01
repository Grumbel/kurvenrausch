#pragma once
#include <SDL2/SDL.h>

namespace racer {

struct InputState {
    bool up = false;
    bool down = false;
    bool left = false;
    bool right = false;
    bool quit = false;
    bool restart = false;
};

class Input {
public:
    void poll(InputState& state);
};

} // namespace racer
