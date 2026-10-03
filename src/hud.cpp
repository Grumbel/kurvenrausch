// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "hud.hpp"

#include "drivetrain.hpp"
#include "driving.hpp"
#include "font.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace racer {

namespace {

constexpr Color Label{0xff, 0xd8, 0x30};
constexpr Color Value{0xf8, 0xf8, 0xf8};
constexpr Color Shadow{0x10, 0x10, 0x20};

constexpr float top_speed_kmh = 293.f;

void text(Framebuffer& fb, int x, int y, std::string_view s, Color c, int scale = 1) {
    fb.draw_text(x + 1, y + 1, s, Shadow, scale);
    fb.draw_text(x, y, s, c, scale);
}

void text_right(Framebuffer& fb, int right, int y, std::string_view s, Color c, int scale = 1) {
    text(fb, right - font::text_width(s, scale), y, s, c, scale);
}

void text_center(Framebuffer& fb, int y, std::string_view s, Color c, int scale = 1) {
    text(fb, (fb.width() - font::text_width(s, scale)) / 2, y, s, c, scale);
}

// The dashboard lamps above the rev counter, shown only while lit: green
// arrows for the indicators, a blue headlight symbol.
void draw_lamps(Framebuffer& fb, int x, int y, const HudState& hud) {
    const Color green{0x40, 0xf0, 0x60}, blue{0x50, 0x90, 0xff};
    auto arrow = [&](int ax, int dir) { // 9 wide, 7 high, pointing `dir`
        for (int i = 0; i < 4; ++i) {
            const int col = dir < 0 ? ax + i : ax + 8 - i;
            fb.fill_rect(col + 1, y + 3 - i, 1, 2 * i + 1, Shadow);
            fb.fill_rect(col, y + 3 - i, 1, 2 * i + 1, green);
        }
        fb.fill_rect(dir < 0 ? ax + 4 : ax, y + 2, 5, 3, green);
    };
    if (hud.signal_left) arrow(x, -1);
    if (hud.signal_right) arrow(x + 12, 1);
    if (hud.headlights) {
        const int hx = x + 26;
        fb.fill_rect(hx + 4, y, 4, 7, blue);   // the lamp
        fb.fill_rect(hx + 8, y + 1, 1, 5, blue);
        for (int r = 0; r < 3; ++r) fb.fill_rect(hx, y + 1 + 2 * r, 3, 1, blue); // its beams
    }
}

// Segmented rev counter. Speed is split into virtual gears; the needle
// climbs through each gear and drops back on the shift.
void draw_tacho(Framebuffer& fb, int x, int y, float speed_fraction) {
    const float rpm = drivetrain::rpm(speed_fraction);

    constexpr int segments = 20;
    fb.fill_rect(x - 1, y - 1, segments * 4 + 1, 8, Shadow);
    for (int i = 0; i < segments; ++i) {
        const float t = static_cast<float>(i) / segments;
        const bool on = t < rpm;
        Color c = t < 0.6f ? Color{0x30, 0xe0, 0x40} : t < 0.85f ? Color{0xf8, 0xd0, 0x20} : Color{0xf0, 0x30, 0x20};
        if (!on) c = blend(c, Shadow, 0.75f);
        fb.fill_rect(x + i * 4, y, 3, 6, c);
    }
}

// Mini map in a size x size box at (x, y): the track as a light line with a
// dark border, the start line, the gas stations and a blinking dot for the car.
void draw_minimap(Framebuffer& fb, int x, int y, int size, const HudState& hud) {
    const std::vector<MapPoint>& map = *hud.map;
    const int n = static_cast<int>(map.size());
    if (n == 0) return;
    for (int j = y; j < y + size; ++j)
        for (int i = x; i < x + size; ++i) fb.blend_pixel(i, j, Shadow, 0.45f);

    // The whole lap fits the box at zoom 1; zoomed in, the box follows the
    // car and cuts off the rest.
    const float inner = static_cast<float>(size - 6) * hud.map_zoom;
    const MapPoint& here = map[static_cast<size_t>(((hud.map_player % n) + n) % n)];
    const float cx0 = hud.map_zoom > 1.f ? static_cast<float>(size) / 2.f - here.x * inner : 3.f;
    const float cy0 = hud.map_zoom > 1.f ? static_cast<float>(size) / 2.f - here.y * inner : 3.f;
    auto px = [&](int seg) {
        const MapPoint& p = map[static_cast<size_t>(((seg % n) + n) % n)];
        return std::make_pair(x + static_cast<int>(std::lround(cx0 + p.x * inner)),
                              y + static_cast<int>(std::lround(cy0 + p.y * inner)));
    };
    fb.set_clip(x, y, x + size, y + size);
    const int step = std::max(1, n / 240);
    for (int pass = 0; pass < 2; ++pass) {
        for (int i = 0; i < n; i += step) {
            const auto [x0, y0] = px(i);
            const auto [x1, y1] = px(i + step);
            if (pass == 0) {
                for (int d = -1; d <= 1; d += 2) {
                    fb.line(x0 + d, y0, x1 + d, y1, Shadow);
                    fb.line(x0, y0 + d, x1, y1 + d, Shadow);
                }
            } else {
                fb.line(x0, y0, x1, y1, Color{0xd8, 0xdc, 0xe4});
            }
        }
    }
    auto dot = [&](int seg, Color c, Color centre) {
        const auto [cx, cy] = px(seg);
        fb.fill_rect(cx - 2, cy - 2, 5, 5, Shadow);
        fb.fill_rect(cx - 1, cy - 1, 3, 3, c);
        fb.put_pixel(cx, cy, centre);
    };
    {   // Chequered start line.
        const auto [cx, cy] = px(hud.map_start);
        fb.fill_rect(cx - 2, cy - 1, 4, 3, Color{0x10, 0x10, 0x10});
        fb.put_pixel(cx - 2, cy - 1, Value); fb.put_pixel(cx, cy - 1, Value);
        fb.put_pixel(cx - 1, cy, Value); fb.put_pixel(cx + 1, cy, Value);
        fb.put_pixel(cx - 2, cy + 1, Value); fb.put_pixel(cx, cy + 1, Value);
    }
    if (hud.map_lots) {
        // Per Lot kind: the dot and its centre.
        static constexpr Color colors[][2] = {
            {{0x30, 0x90, 0xf0}, {0xb0, 0xe0, 0xff}}, // gas station
            {{0xf8, 0xd0, 0x20}, {0xff, 0xf4, 0xc0}}, // car dealer
            {{0x30, 0xc8, 0xc0}, {0xc0, 0xff, 0xf8}}, // car wash
            {{0xf0, 0x40, 0x90}, {0xff, 0xc0, 0xe0}}, // motel
            {{0xf0, 0xf0, 0xf0}, {0xe0, 0x20, 0x20}}, // hospital
            {{0xf0, 0x80, 0x20}, {0xff, 0xd0, 0x90}}, // truck stop
        };
        static_assert(sizeof(colors) / sizeof(colors[0]) == static_cast<size_t>(lot_kinds), "every lot needs its colours");
        for (size_t k = 0; k < hud.map_lots->size(); ++k) {
            for (int s : (*hud.map_lots)[k]) dot(s, colors[k][0], colors[k][1]);
        }
    }
    const Color car{0xf0, 0x30, 0x20};
    dot(hud.map_player, car, hud.map_blink ? Value : car);
    fb.reset_clip();
}

// Fuel gauge: a pump symbol and a bar, red and blinking when low.
void draw_fuel(Framebuffer& fb, int x, int y, float level, bool warning) {
    const Color icon = warning ? Color{0xf0, 0x30, 0x20} : Label;
    fb.fill_rect(x, y + 1, 5, 7, icon); // the pump
    fb.fill_rect(x + 1, y + 2, 3, 2, Shadow);
    fb.fill_rect(x + 5, y + 2, 1, 1, icon); // hose
    fb.fill_rect(x + 6, y + 3, 1, 4, icon);
    const int bx = x + 9, w = 21;
    fb.fill_rect(bx - 1, y + 1, w + 2, 7, Shadow);
    const int filled = static_cast<int>(std::lround(std::clamp(level, 0.f, 1.f) * static_cast<float>(w)));
    const Color c = level < Fuel::low ? Color{0xf0, 0x30, 0x20} : level < 0.5f ? Color{0xf8, 0xd0, 0x20}
                                                                                : Color{0x30, 0xe0, 0x40};
    if (filled > 0 && !(warning && level <= 0.f)) fb.fill_rect(bx, y + 2, filled, 5, c);
}

// Nitro canisters, right-aligned at `right`: full ones, the one burning now
// draining, and the empty ones.
void draw_nitro(Framebuffer& fb, int right, int y, int full, float burning) {
    constexpr int count = Nitro::capacity, w = 7, gap = 3, h = 14;
    const Color glass{0x1c, 0x24, 0x3c}, fill{0x30, 0x90, 0xf0}, shine{0xb0, 0xe0, 0xff}, cap{0xc8, 0xc8, 0xd0};
    for (int i = 0; i < count; ++i) {
        const int x = right - (count - i) * (w + gap) + gap;
        fb.fill_rect(x - 1, y + 2, w + 2, h - 1, Shadow);
        fb.fill_rect(x + 2, y, 3, 3, cap);
        fb.fill_rect(x, y + 3, w, h - 3, glass);
        float level = i < full ? 1.f : (i == full ? burning : 0.f);
        const int filled = static_cast<int>(std::lround(level * static_cast<float>(h - 4)));
        if (filled > 0) {
            fb.fill_rect(x + 1, y + h - 1 - filled, w - 2, filled, fill);
            fb.fill_rect(x + 1, y + h - 1 - filled, 1, filled, shine);
        }
    }
}

} // namespace

std::string format_lap_time(float seconds) {
    const int cs = static_cast<int>(std::max(0.f, seconds) * 100.f);
    char buf[32];
    std::snprintf(buf, sizeof buf, "%d'%02d\"%02d", cs / 6000, (cs / 100) % 60, cs % 100);
    return buf;
}

void draw_hud(Framebuffer& fb, const HudState& hud) {
    const int w = fb.width();
    const int h = fb.height();

    // Top left: current lap time.
    text(fb, 6, 5, "TIME", Label);
    text(fb, 6, 14, format_lap_time(hud.lap_time), Value, 2);

    // Below it the lap counter; the top centre holds the rear-view mirror.
    text(fb, 6, 33, "LAP", Label);
    text(fb, 6, 42, hud.lap > 0 ? std::to_string(hud.lap) : "-", Value, 2);

    // Top right: last and best laps.
    text_right(fb, w - 6, 5, "BEST", Label);
    text_right(fb, w - 6, 14, hud.best_lap > 0.f ? format_lap_time(hud.best_lap) : "-'--\"--", Value);
    text_right(fb, w - 6, 25, "LAST", Label);
    text_right(fb, w - 6, 34, hud.last_lap > 0.f ? format_lap_time(hud.last_lap) : "-'--\"--", Value);

    // Below them, the mini map.
    if (hud.map) draw_minimap(fb, w - 6 - 58, 46, 58, hud);

    // At a lot with a choice: what is on offer, for a car with bars for its
    // strengths.
    int message_y = h / 2 - 42;
    if (!hud.offer_title.empty()) {
        const int px = 70, py = 68, pw = w - 140, ph = hud.offer_stats ? 70 : 36;
        message_y = py + ph + 6; // below the panel, not over it
        for (int y = py; y < py + ph; ++y)
            for (int x = px; x < px + pw; ++x) fb.blend_pixel(x, y, Shadow, 0.7f);
        text_center(fb, py + 5, hud.offer_title, Label);
        text_center(fb, py + 16, "< " + hud.offer_name + " >", Value, 2);
        const char* labels[3] = {"SPEED", "ACCEL", "GRIP"};
        for (int i = 0; hud.offer_stats && i < 3; ++i) {
            const int y = py + 36 + i * 10;
            text(fb, px + 8, y, labels[i], Label);
            const int bar = static_cast<int>(std::lround(std::clamp((hud.offer_values[i] - 0.6f) / 0.75f, 0.f, 1.f) * 90.f));
            fb.fill_rect(px + 50, y, 90, 7, Color{0x30, 0x30, 0x40});
            fb.fill_rect(px + 50, y, bar, 7, Color{0x30, 0xe0, 0x40});
        }
    }

    // A fork ahead: which way goes where.
    if (!hud.fork_left.empty()) {
        text(fb, 6, 112, "< " + hud.fork_left, Value);
        text_right(fb, w - 6, 112, hud.fork_right + " >", Value);
    }

    // Bottom left: speed and revs.
    const int kmh = static_cast<int>(std::lround(hud.speed_kmh_fraction * top_speed_kmh));
    text_right(fb, 52, h - 28, std::to_string(kmh), Value, 3);
    text(fb, 56, h - 14, "KM/H", Label);
    if (hud.reverse) text(fb, 86, h - 21, "R", Value, 2);
    draw_tacho(fb, 6, h - 37, hud.speed_fraction);
    draw_lamps(fb, 6, h - 47, hud);
    draw_fuel(fb, 56, h - 28, hud.fuel, hud.fuel_warning);

    // Bottom right: nitro.
    text_right(fb, w - 6, h - 37, "NITRO", Label);
    draw_nitro(fb, w - 6, h - 27, hud.nitro, hud.nitro_burn);

    if (hud.muted) text_right(fb, w - 6, h - 9, "MUTE", Label);

    if (!hud.banner.empty()) {
        text_center(fb, 48, hud.banner, Value, 2);
        text_center(fb, 65, hud.banner_sub, Label);
    }

    if (!hud.message.empty() && hud.message_visible) {
        text_center(fb, message_y, hud.message, Label, 3);
    }
}

void draw_pause_menu(Framebuffer& fb, const PauseMenu& menu, const std::string& country) {
    for (int y = 0; y < fb.height(); ++y) {
        for (int x = 0; x < fb.width(); ++x) fb.blend_pixel(x, y, Shadow, 0.55f);
    }
    const int top = fb.height() / 2 - 50;
    text_center(fb, top, "PAUSED", Value, 3);
    const std::string items[PauseMenu::items] = {"RESUME", "RESTART", "START IN < " + country + " >", "QUIT"};
    for (int i = 0; i < menu.item_count(); ++i) {
        const bool on = i == menu.selected;
        const std::string line = on ? "> " + items[i] + " <" : items[i];
        text_center(fb, top + 36 + 16 * i, line, on ? Label : Value);
    }
}

} // namespace racer
