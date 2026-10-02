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
              << "  --wav FILE          also write the sound of the simulated run (needs --screenshot)\n"
              << "  --zone N            start the screenshot inside zone N (see --print-zones)\n"
              << "  --print-zones       list the zones of the track and exit\n"
              << "  --steer S           hold steering at S (-1 .. 1) instead of the autopilot\n"
              << "  --steer-from N      start holding the steering at step N (default 0)\n"
              << "  --horn              hold the horn during the headless run\n"
              << "  --fuel L            start the headless run with L (0 .. 1) of a tank\n"
              << "  --car N             drive car model N (0 Spider, 1 GT Coupe, 2 Hot Hatch, 3 Muscle, 4 Big Rig)\n"
              << "  --dealer            pull in at the next car dealer during the headless run\n"
              << "  --handbrake N       hold the handbrake from step N of the headless run\n"
              << "  --brake N           brake from step N to a stop, let go, then hold it: reverse\n"
              << "  --pause             show the pause menu in the screenshot\n"
              << "  --dirt L            start the headless run this dirty (0 to 1)\n"
              << "  --wash              pull in at the next car wash during the headless run\n"
              << "  --visit KIND        pull in at the next lot of KIND (gas, dealer, wash, motel, hospital, truckstop)\n"
              << "  --nitro N           press nitro at step N of the headless run\n"
              << "  --icon FILE         write the application icon (32x32) as a BMP and exit\n"
              << "  --version           print the version and exit\n"
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
        } else if (arg == "--wav" && i + 1 < argc) {
            shot.wav_path = argv[++i];
        } else if (arg == "--zone" && i + 1 < argc) {
            shot.zone = std::atoi(argv[++i]);
        } else if (arg == "--print-zones") {
            print_zones = true;
        } else if (arg == "--steer" && i + 1 < argc) {
            shot.force_steer = true;
            shot.steer = static_cast<float>(std::atof(argv[++i]));
        } else if (arg == "--steer-from" && i + 1 < argc) {
            shot.steer_from = std::atoi(argv[++i]);
        } else if (arg == "--car" && i + 1 < argc) {
            shot.car = std::atoi(argv[++i]);
        } else if (arg == "--brake" && i + 1 < argc) {
            shot.brake_from = std::atoi(argv[++i]);
        } else if (arg == "--handbrake" && i + 1 < argc) {
            shot.handbrake_from = std::atoi(argv[++i]);
        } else if (arg == "--dirt" && i + 1 < argc) {
            shot.dirt = static_cast<float>(std::atof(argv[++i]));
        } else if (arg == "--wash") {
            shot.visit = static_cast<int>(racer::Lot::Wash);
        } else if (arg == "--visit" && i + 1 < argc) {
            const std::string kind = argv[++i];
            for (int k = 0; k < racer::lot_kinds; ++k) {
                if (kind == racer::lot_keyword(static_cast<racer::Lot>(k))) shot.visit = k;
            }
            if (shot.visit < 0) {
                std::cerr << "Unknown lot for --visit: " << kind << "\n";
                return 1;
            }
        } else if (arg == "--pause") {
            shot.pause = true;
        } else if (arg == "--dealer") {
            shot.visit = static_cast<int>(racer::Lot::Dealer);
        } else if (arg == "--fuel" && i + 1 < argc) {
            shot.fuel = static_cast<float>(std::atof(argv[++i]));
        } else if (arg == "--horn") {
            shot.horn = true;
        } else if (arg == "--nitro" && i + 1 < argc) {
            shot.nitro_frame = std::atoi(argv[++i]);
        } else if (arg == "--icon" && i + 1 < argc) {
            const racer::Bitmap icon = racer::make_app_icon();
            return racer::save_bmp(argv[++i], icon.px.data(), icon.w, icon.h) ? 0 : 1;
        } else if (arg == "--version") {
            std::cout << "kurvenrausch " << KURVENRAUSCH_VERSION << "\n";
            return 0;
        } else if (arg == "--help" || arg == "-h") {
            usage(argv[0]);
            return 0;
        } else {
            std::cerr << "Unknown or incomplete option: " << arg << "\n";
            usage(argv[0]);
            return 1;
        }
    }

    if (!shot.wav_path.empty() && shot.path.empty()) {
        std::cerr << "--wav needs --screenshot: the sound is recorded while the headless run simulates\n";
        return 1;
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
