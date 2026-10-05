// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "sprite_viewer.hpp"

#include "display.hpp"
#include "framebuffer.hpp"
#include "input.hpp"
#include "overlay.hpp"
#include "sprites.hpp"
#include "vehicles.hpp"

#include <SDL2/SDL.h>

#include <algorithm>
#include <string>
#include <vector>

namespace racer {

namespace {

struct Entry {
    std::string category;
    std::string name;
    const Bitmap* bitmap = nullptr;
};

void add(std::vector<Entry>& out, std::string cat, std::string name, const Bitmap& b) {
    out.push_back(Entry{std::move(cat), std::move(name), &b});
}

std::vector<Entry> catalogue(const SpriteSheet& sheet) {
    std::vector<Entry> out;
    out.reserve(800);

    for (int i = 0; i < static_cast<int>(Scenery::Count); ++i)
        add(out, "Scenery", "Scenery " + std::to_string(i), sheet.scenery(static_cast<Scenery>(i)));
    add(out, "Scenery", "Billboard back", sheet.scenery_back(Scenery::Billboard));

    for (int m = 0; m < car_models; ++m) {
        const char* car = car_model(m).name;
        add(out, "Player", std::string(car) + " rear", sheet.player(m, 0, false, 0, 0));
        add(out, "Player", std::string(car) + " brake", sheet.player(m, 0, true, 0, 0));
        add(out, "Player", std::string(car) + " dash", sheet.dashboard(m));
    }
    for (int d = 0; d < drivers; ++d)
        add(out, "Player", "Wheel " + std::to_string(d), sheet.wheel(d));

    static const char* vehicle_names[] = {"Car", "Van", "Truck", "Rival", "Police", "Hatch", "Pickup", "Bus",
                                          "Ambulance"};
    for (int k = 0; k < static_cast<int>(Vehicle::Count); ++k) {
        const auto kind = static_cast<Vehicle>(k);
        const int styles = vehicle_info(kind).styles;
        const char* base = (k < static_cast<int>(sizeof(vehicle_names) / sizeof(vehicle_names[0])))
                               ? vehicle_names[k]
                               : "Vehicle";
        for (int s = 0; s < styles; ++s) {
            add(out, "Traffic", std::string(base) + " s" + std::to_string(s) + " rear",
                sheet.vehicle(kind, s, 0, false, 0));
            add(out, "Traffic", std::string(base) + " s" + std::to_string(s) + " front",
                sheet.vehicle_front(kind, s, 0, 0));
        }
    }

    for (int a = 0; a < animal_kinds; ++a) {
        add(out, "Animals", "Animal " + std::to_string(a) + "a", sheet.animal(static_cast<Animal>(a), 0));
        add(out, "Animals", "Animal " + std::to_string(a) + "b", sheet.animal(static_cast<Animal>(a), 1));
    }
    for (int f = 0; f < fares; ++f)
        add(out, "People", "Fare " + std::to_string(f), sheet.pedestrian(f, 0));
    for (int t = 0; t < train_wagon_kinds + 1; ++t)
        add(out, "Train", t == 0 ? std::string("Loco") : "Wagon " + std::to_string(t), sheet.train_car(t));
    add(out, "Props", "Ramp truck", sheet.ramp_truck(0));
    add(out, "Props", "Crossing L", sheet.crossing_sign(-1));
    add(out, "Props", "Crossing R", sheet.crossing_sign(+1));
    return out;
}

void draw_chequer(Framebuffer& fb) {
    const Color a{0x28, 0x28, 0x30}, b{0x38, 0x38, 0x42};
    for (int y = 0; y < fb.height(); ++y)
        for (int x = 0; x < fb.width(); ++x)
            fb.put_pixel(x, y, ((x / 8) ^ (y / 8)) & 1 ? a : b);
}

} // namespace

bool run_sprite_viewer_session(Display& display, Input& input, const SpriteSheet& sheet) {
    const std::vector<Entry> entries = catalogue(sheet);
    if (entries.empty()) return false;

    // Match the display texture size. A fixed 320x240 buffer fed into a wider
    // (or taller HD) game texture makes SDL_UpdateTexture use the wrong pitch
    // and the picture tears into diagonal stripes.
    const int fb_w = std::max(1, display.fb_width());
    const int fb_h = std::max(1, display.fb_height());
    Framebuffer fb(fb_w, fb_h);
    Overlay overlay;
    int index = 0;
    bool running = true;

    const auto jump_category = [&](int dir) {
        const std::string start_cat = entries[static_cast<size_t>(index)].category;
        int i = index;
        for (int n = 0; n < static_cast<int>(entries.size()); ++n) {
            i = (i + dir + static_cast<int>(entries.size())) % static_cast<int>(entries.size());
            if (entries[static_cast<size_t>(i)].category != start_cat) break;
        }
        const std::string neu = entries[static_cast<size_t>(i)].category;
        while (i > 0 && entries[static_cast<size_t>(i - 1)].category == neu) --i;
        index = i;
    };

    while (running) {
        InputState st;
        input.poll(st);
        // Esc, Start (pause), B (menu.back), or window close leave the browser.
        if (st.quit || st.escape || st.pause || st.menu.back) running = false;
        if (st.menu.left)
            index = (index - 1 + static_cast<int>(entries.size())) % static_cast<int>(entries.size());
        if (st.menu.right) index = (index + 1) % static_cast<int>(entries.size());
        if (st.menu.up) jump_category(-1);
        if (st.menu.down) jump_category(+1);
        if (st.toggle_fullscreen) display.toggle_fullscreen();

        draw_chequer(fb);
        const Entry& e = entries[static_cast<size_t>(index)];
        const Bitmap& bmp = *e.bitmap;
        fb.blit_scaled(bmp, static_cast<float>((fb_w - bmp.w) / 2), static_cast<float>((fb_h - bmp.h) / 2 - 10),
                       static_cast<float>(bmp.w), static_cast<float>(bmp.h));
        fb.draw_text(4, 4, e.category + " / " + e.name, Color{0xf0, 0xf0, 0xf0});
        fb.draw_text(4, 14,
                     std::to_string(index + 1) + "/" + std::to_string(entries.size()) + "  " + std::to_string(bmp.w) +
                         "x" + std::to_string(bmp.h),
                     Color{0xa0, 0xa8, 0xb0});
        fb.draw_text(4, fb_h - 12, "LEFT/RIGHT  UP/DOWN category  ESC/START back", Color{0x80, 0x88, 0x90});

        display.present(fb.pixels(), overlay);
        SDL_Delay(16);
    }
    return true;
}

bool run_sprite_viewer(bool fullscreen) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_EVENTS | SDL_INIT_TIMER) != 0) return false;
    Display display;
    if (!display.init("Kurvenrausch sprites", "io.github.grumbel.kurvenrausch", 320, 240, 3, fullscreen)) {
        SDL_Quit();
        return false;
    }
    Input input;
    input.init();
    const SpriteSheet sheet;
    const bool ok = run_sprite_viewer_session(display, input, sheet);
    SDL_Quit();
    return ok;
}

} // namespace racer
