// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "game.hpp"
#include "sprite_viewer.hpp"

#include <SDL2/SDL.h>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

namespace {

#ifdef __EMSCRIPTEN__
racer::Game* web_game = nullptr; // for the web page's hooks
#endif

void usage(const char* argv0) {
    std::cout << "Usage: " << argv0 << " [OPTIONS]\n"
              << "  --fullscreen        start covering the whole screen\n"
              << "  --renderer MODE     road scene: auto, software, or gles (F8 cycles)\n"
              << "  --screenshot FILE   render headless and save a BMP, then exit\n"
              << "  --frames N          simulation steps (60/s) before the screenshot (default 0)\n"
              << "  --position Z        start distance along the track for the screenshot\n"
              << "  --wav FILE          also write the sound of the simulated run (needs --screenshot)\n"
              << "  --zone N            start the screenshot inside zone N (see --print-zones)\n"
              << "  --track N           drive track N for the screenshot or --print-zones (0 small world, 1 grand tour)\n"
              << "  --print-zones       list the zones of the track and exit\n"
              << "  --steer S           hold steering at S (-1 .. 1) instead of the autopilot\n"
              << "  --steer-from N      start holding the steering at step N (default 0)\n"
              << "  --horn              hold the horn during the headless run\n"
              << "  --fuel L            start the headless run with L (0 .. 1) of a tank\n"
              << "  --car N             drive car model N (0 Spider, 1 GT Coupe, 2 Hot Hatch, 3 Muscle, 4 Big Rig,\n"
              << "                      5 Saloon, 6 Taxi, 7 Estate, 8 Patrol, 9 Racer, 10 Van, 11 Box Truck)\n"
              << "  --dealer            pull in at the next car dealer during the headless run\n"
              << "  --handbrake N       hold the handbrake from step N of the headless run\n"
              << "  --brake N           brake from step N to a stop, let go, then hold it: reverse\n"
              << "  --pause             show the pause menu in the screenshot\n"
              << "  --view N            camera view (0 chase, 1 far, 2 bumper, 3 cockpit)\n"
              << "  --storm L           hold the weather at L (0 clear to 1 storm)\n"
              << "  --music N           play radio track N (0 to 2) in the --wav recording\n"
              << "  --police N          start a police chase at step N of the headless run\n"
              << "  --touch X,Y         hold a finger at X,Y (framebuffer pixels) all through the headless run\n"
              << "  --signal N          indicators on in the headless run: -1 left, 1 right, 2 hazard lights\n"
              << "  --headlights        headlights on in the headless run\n"
              << "  --hour H            start the headless run at H o'clock (0 to 24)\n"
              << "  --attract           show the attract mode (following the traffic) in the screenshot\n"
              << "  --dirt L            start the headless run this dirty (0 to 1)\n"
              << "  --width W           make the screenshot W pixels wide (320, 4:3, to 640), 240 high\n"
              << "  --wash              pull in at the next car wash during the headless run\n"
              << "  --visit KIND        pull in at the next lot of KIND (gas, dealer, sports, wash, motel, hospital, truckstop, chemical)\n"
              << "  --nitro N           press nitro at step N of the headless run\n"
              << "  --icon FILE         write the application icon (32x32) as a BMP and exit\n"
              << "  --version           print the version and exit\n"
              << "  --sprites           browse every sprite and exit\n"              << "  --help              show this help\n";
}

} // namespace

int main(int argc, char* argv[]) {
    racer::ScreenshotOptions shot;
    racer::SceneBackend scene_backend = racer::SceneBackend::Auto;
    bool scene_backend_set = false;
    bool print_zones = false;
    bool fullscreen = false;

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
        } else if (arg == "--track" && i + 1 < argc) {
            shot.track = std::atoi(argv[++i]);
        } else if (arg == "--print-zones") {
            print_zones = true;
        } else if (arg == "--renderer" && i + 1 < argc) {
            if (!racer::parse_scene_backend(argv[++i], scene_backend)) {
                std::cerr << "unknown --renderer (use auto, software, or gles)\n";
                return 1;
            }
            scene_backend_set = true;
        } else if (arg == "--fullscreen") {
            fullscreen = true;
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
        } else if (arg == "--width" && i + 1 < argc) {
            shot.width = std::atoi(argv[++i]);
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
        } else if (arg == "--signal" && i + 1 < argc) {
            shot.signal = std::atoi(argv[++i]);
        } else if (arg == "--attract") {
            shot.attract = true;
        } else if (arg == "--hour" && i + 1 < argc) {
            shot.hour = static_cast<float>(std::atof(argv[++i]));
        } else if (arg == "--headlights") {
            shot.headlights = true;
        } else if (arg == "--touch" && i + 1 < argc) {
            const std::string at = argv[++i];
            const size_t comma = at.find(',');
            if (comma == std::string::npos) {
                std::cerr << "--touch wants X,Y\n";
                return 1;
            }
            shot.touches.push_back(racer::Finger{static_cast<int64_t>(shot.touches.size()),
                                                 static_cast<float>(std::atof(at.substr(0, comma).c_str())),
                                                 static_cast<float>(std::atof(at.substr(comma + 1).c_str()))});
        } else if (arg == "--police" && i + 1 < argc) {
            shot.police_frame = std::atoi(argv[++i]);
        } else if (arg == "--music" && i + 1 < argc) {
            shot.music = std::atoi(argv[++i]);
        } else if (arg == "--storm" && i + 1 < argc) {
            shot.storm = static_cast<float>(std::atof(argv[++i]));
        } else if (arg == "--view" && i + 1 < argc) {
            shot.view = std::atoi(argv[++i]);
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
        } else if (arg == "--sprites") {
            return racer::run_sprite_viewer(fullscreen) ? 0 : 1;
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

    // On the heap: in a web page the browser keeps calling into the game
    // after main() has handed it the loop.
    auto game = std::make_unique<racer::Game>();
    if (shot.track != 0) game->load_track(shot.track);

    if (print_zones) {
        game->print_zones();
        return 0;
    }

    if (!shot.path.empty()) {
        return game->screenshot(shot) ? 0 : 1;
    }

    if (!game->init(fullscreen)) {
        std::cerr << "Failed to initialize game.\n";
        return 1;
    }
#ifdef __EMSCRIPTEN__
    web_game = game.release(); // lives as long as the page
    web_game->run();
#else
    game->run();
#endif
    return 0;
}

#ifdef __EMSCRIPTEN__
// Called by the web page when the tab is hidden or its Pause button pressed.
extern "C" EMSCRIPTEN_KEEPALIVE void kurvenrausch_pause() {
    if (web_game) web_game->pause();
}
#endif
