// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "game.hpp"

#include <SDL2/SDL.h>
#include <cstdlib>
#include <iostream>
#include <string>

namespace {

void usage(const char* argv0) {
    std::cout << "Usage: " << argv0 << " [OPTIONS]\n"
              << "  --screenshot FILE   render headless and save a BMP, then exit\n"
              << "  --frames N          simulation steps (60/s) before the screenshot (default 0)\n"
              << "  --position Z        start distance along the track for the screenshot\n"
              << "  --zone N            start the screenshot inside zone N (see --print-zones)\n"
              << "  --print-zones       list the zones of the track and exit\n"
              << "  --steer S           hold steering at S (-1 .. 1) instead of the autopilot\n"
              << "  --help              show this help\n";
}

} // namespace

int main(int argc, char* argv[]) {
    racer::ScreenshotOptions shot;
    bool print_zones = false;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--screenshot" && i + 1 < argc) {
            shot.path = argv[++i];
        } else if (arg == "--frames" && i + 1 < argc) {
            shot.frames = std::atoi(argv[++i]);
        } else if (arg == "--position" && i + 1 < argc) {
            shot.position = static_cast<float>(std::atof(argv[++i]));
        } else if (arg == "--zone" && i + 1 < argc) {
            shot.zone = std::atoi(argv[++i]);
        } else if (arg == "--print-zones") {
            print_zones = true;
        } else if (arg == "--steer" && i + 1 < argc) {
            shot.force_steer = true;
            shot.steer = static_cast<float>(std::atof(argv[++i]));
        } else if (arg == "--help" || arg == "-h") {
            usage(argv[0]);
            return 0;
        } else {
            std::cerr << "Unknown or incomplete option: " << arg << "\n";
            usage(argv[0]);
            return 1;
        }
    }

    racer::Game game;

    if (print_zones) {
        game.print_zones();
        return 0;
    }

    if (!shot.path.empty()) {
        return game.screenshot(shot) ? 0 : 1;
    }

    if (!game.init()) {
        std::cerr << "Failed to initialize game.\n";
        return 1;
    }
    game.run();
    return 0;
}
