#include "game.hpp"
#include <iostream>

int main(int /*argc*/, char* /*argv*/[]) {
    racer::Game game(1024, 768);
    if (!game.init()) {
        std::cerr << "Failed to initialize game.\n";
        return 1;
    }
    game.run();
    game.shutdown();
    return 0;
}
