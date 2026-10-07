// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "hud.hpp"

#include "drivetrain.hpp"
#include "driving.hpp"
#include "font.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <deque>

namespace racer {

namespace {

constexpr Color Label{0xff, 0xd8, 0x30};
constexpr Color Value{0xf8, 0xf8, 0xf8};
constexpr Color Shadow{0x10, 0x10, 0x20};

// Full-screen dim behind the menus.
void dim_menu_bg(Canvas& fb) {
    fb.blend_rect(0, 0, fb.width(), fb.height(), Color{0x10, 0x10, 0x20}, 140.f / 255.f);
}

void text(Canvas& fb, int x, int y, std::string_view s, Color c, int scale = 1) {
    fb.draw_text(x + 1, y + 1, s, Shadow, scale);
    fb.draw_text(x, y, s, c, scale);
}

void text_right(Canvas& fb, int right, int y, std::string_view s, Color c, int scale = 1) {
    text(fb, right - font::text_width(s, scale), y, s, c, scale);
}

void text_center(Canvas& fb, int y, std::string_view s, Color c, int scale = 1) {
    text(fb, (fb.width() - font::text_width(s, scale)) / 2, y, s, c, scale);
}

// The dashboard lamps above the rev counter: green arrows for the indicators,
// a blue headlight symbol. Always drawn; off lamps are greyed out so the
// driver can still see where they sit.
void draw_lamps(Canvas& fb, int x, int y, const HudState& hud, int s) {
    const Color green{0x40, 0xf0, 0x60}, blue{0x50, 0x90, 0xff};
    const Color off{0x50, 0x54, 0x60}; // dim when the lamp is not lit
    auto arrow = [&](int ax, int dir, bool on) { // 9×s wide, 7×s high, pointing `dir`
        // The whole shape in shadow a pixel down and right, then in colour:
        // drawn column by column, a shadow would cover the fill beside it.
        for (int pass = 0; pass < 2; ++pass) {
            const int o = pass == 0 ? s : 0;
            const Color c = pass == 0 ? Shadow : (on ? green : off);
            for (int i = 0; i < 4; ++i) {
                const int col = dir < 0 ? ax + i * s : ax + (8 - i) * s;
                fb.fill_rect(col + o, y + (3 - i) * s + o, s, (2 * i + 1) * s, c);
            }
            fb.fill_rect((dir < 0 ? ax + 4 * s : ax) + o, y + 2 * s + o, 5 * s, 3 * s, c);
        }
    };
    arrow(x, -1, hud.signal_left);
    arrow(x + 12 * s, 1, hud.signal_right);
    {
        const int hx = x + 26 * s;
        const Color c = hud.headlights ? blue : off;
        fb.fill_rect(hx + 4 * s, y, 4 * s, 7 * s, c); // the lamp
        fb.fill_rect(hx + 8 * s, y + 1 * s, 1 * s, 5 * s, c);
        for (int r = 0; r < 3; ++r) fb.fill_rect(hx, y + (1 + 2 * r) * s, 3 * s, 1 * s, c); // beams
    }
}

// Segmented rev counter. Speed is split into virtual gears; the needle
// climbs through each gear and drops back on the shift.
void draw_tacho(Canvas& fb, int x, int y, float speed_fraction, int s) {
    const float rpm = drivetrain::rpm(speed_fraction);

    constexpr int segments = 20;
    fb.fill_rect(x - 1 * s, y - 1 * s, segments * 4 * s + 1 * s, 8 * s, Shadow);
    for (int i = 0; i < segments; ++i) {
        const float t = static_cast<float>(i) / segments;
        const bool on = t < rpm;
        Color c = t < 0.6f ? Color{0x30, 0xe0, 0x40} : t < 0.85f ? Color{0xf8, 0xd0, 0x20} : Color{0xf0, 0x30, 0x20};
        if (!on) c = blend(c, Shadow, 0.75f);
        fb.fill_rect(x + i * 4 * s, y, 3 * s, 6 * s, c);
    }
}

// The track of the mini map at `inner` pixels across, drawn once: a light
// line with a dark border, minimap_margin pixels in from the bitmap's edges.
constexpr int minimap_margin = 3;
const Bitmap& minimap_layer(const std::vector<MapPoint>& map, float inner) {
    struct Entry {
        const MapPoint* data;
        size_t n;
        float inner, x0, y0;
        Bitmap bitmap;
    };
    static std::deque<Entry> cache; // stable references: draw lists keep them a frame
    const float x0 = map.empty() ? 0.f : map.front().x, y0 = map.empty() ? 0.f : map.front().y;
    for (const Entry& e : cache)
        if (e.data == map.data() && e.n == map.size() && e.inner == inner && e.x0 == x0 && e.y0 == y0) return e.bitmap;
    const int n = static_cast<int>(map.size());
    const int extent = static_cast<int>(std::lround(inner)) + 2 * minimap_margin + 1;
    Framebuffer fb(extent, extent); // transparent
    auto px = [&](int seg) {
        const MapPoint& p = map[static_cast<size_t>(((seg % n) + n) % n)];
        return std::make_pair(minimap_margin + static_cast<int>(std::lround(p.x * inner)),
                              minimap_margin + static_cast<int>(std::lround(p.y * inner)));
    };
    const int step = std::max(1, n / 240);
    for (int pass = 0; pass < 2; ++pass) {
        for (int i = 0; i < n; i += step) {
            const auto [ax, ay] = px(i);
            const auto [bx, by] = px(i + step);
            if (pass == 0) {
                for (int d = -1; d <= 1; d += 2) {
                    fb.line(ax + d, ay, bx + d, by, Shadow);
                    fb.line(ax, ay + d, bx, by + d, Shadow);
                }
            } else {
                fb.line(ax, ay, bx, by, Color{0xd8, 0xdc, 0xe4});
            }
        }
    }
    Bitmap b(extent, extent);
    b.px.assign(fb.pixels(), fb.pixels() + b.px.size());
    cache.push_back(Entry{map.data(), map.size(), inner, x0, y0, std::move(b)});
    return cache.back().bitmap;
}

// Mini map in a size x size box at (x, y): the track as a light line with a
// dark border, the start line, the gas stations and a blinking dot for the car.
void draw_minimap(Canvas& fb, int x, int y, int size, const HudState& hud) {
    const std::vector<MapPoint>& map = *hud.map;
    const int n = static_cast<int>(map.size());
    if (n == 0) return;
    fb.blend_rect(x, y, size, size, Shadow, 0.45f);

    // The whole lap fits the box at zoom 1; zoomed in, the box follows the
    // car and cuts off the rest. The track itself is a cached bitmap.
    const float inner = static_cast<float>(size - 6) * hud.map_zoom;
    const MapPoint& here = map[static_cast<size_t>(((hud.map_player % n) + n) % n)];
    const int ox = hud.map_zoom > 1.f ? static_cast<int>(std::lround(static_cast<float>(size) / 2.f - here.x * inner)) : 3;
    const int oy = hud.map_zoom > 1.f ? static_cast<int>(std::lround(static_cast<float>(size) / 2.f - here.y * inner)) : 3;
    auto px = [&](int seg) {
        const MapPoint& p = map[static_cast<size_t>(((seg % n) + n) % n)];
        return std::make_pair(x + ox + static_cast<int>(std::lround(p.x * inner)),
                              y + oy + static_cast<int>(std::lround(p.y * inner)));
    };
    fb.set_clip(x, y, x + size, y + size);
    const Bitmap& track = minimap_layer(map, inner);
    fb.blit(track, static_cast<float>(x + ox - minimap_margin), static_cast<float>(y + oy - minimap_margin),
            static_cast<float>(track.w), static_cast<float>(track.h));
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
            {{0xe0, 0x20, 0x30}, {0xff, 0xb0, 0xb0}}, // sports car dealer
            {{0x40, 0xe0, 0x60}, {0xd0, 0xff, 0xd0}}, // chemical plant
            {{0xb0, 0x60, 0xf0}, {0xe8, 0xd0, 0xff}}, // the player's garage
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
void draw_fuel(Canvas& fb, int x, int y, float level, bool warning, int s) {
    const Color icon = warning ? Color{0xf0, 0x30, 0x20} : Label;
    fb.fill_rect(x, y + 1 * s, 5 * s, 7 * s, icon); // the pump
    fb.fill_rect(x + 1 * s, y + 2 * s, 3 * s, 2 * s, Shadow);
    fb.fill_rect(x + 5 * s, y + 2 * s, 1 * s, 1 * s, icon); // hose
    fb.fill_rect(x + 6 * s, y + 3 * s, 1 * s, 4 * s, icon);
    const int bx = x + 9 * s, bw = 21 * s;
    fb.fill_rect(bx - 1 * s, y + 1 * s, bw + 2 * s, 7 * s, Shadow);
    const int filled = static_cast<int>(std::lround(std::clamp(level, 0.f, 1.f) * static_cast<float>(bw)));
    const Color c = level < Fuel::low ? Color{0xf0, 0x30, 0x20} : level < 0.5f ? Color{0xf8, 0xd0, 0x20}
                                                                                : Color{0x30, 0xe0, 0x40};
    if (filled > 0 && !(warning && level <= 0.f)) fb.fill_rect(bx, y + 2 * s, filled, 5 * s, c);
}

// Nitro canisters, right-aligned at `right`: full ones, the one burning now
// draining, and the empty ones.
void draw_nitro(Canvas& fb, int right, int y, int count, int full, float burning, int s) {
    // A touch larger than the old 7×14 so the cans read at SD; then ui_scale.
    const int w = 9 * s, gap = 3 * s, h = 16 * s;
    const Color glass{0x1c, 0x24, 0x3c}, fill{0x30, 0x90, 0xf0}, shine{0xb0, 0xe0, 0xff}, cap{0xc8, 0xc8, 0xd0};
    for (int i = 0; i < count; ++i) {
        const int x = right - (count - i) * (w + gap) + gap;
        fb.fill_rect(x - 1 * s, y + 2 * s, w + 2 * s, h - 1 * s, Shadow);
        fb.fill_rect(x + 2 * s, y, 5 * s, 3 * s, cap);
        fb.fill_rect(x, y + 3 * s, w, h - 3 * s, glass);
        float level = i < full ? 1.f : (i == full ? burning : 0.f);
        const int filled = static_cast<int>(std::lround(level * static_cast<float>(h - 4 * s)));
        if (filled > 0) {
            fb.fill_rect(x + 1 * s, y + h - 1 * s - filled, w - 2 * s, filled, fill);
            fb.fill_rect(x + 1 * s, y + h - 1 * s - filled, 1 * s, filled, shine);
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

void draw_hud(Canvas& fb, const HudState& hud, int ui_scale) {
    const int w = fb.width();
    const int h = fb.height();
    const int s = std::max(1, ui_scale);
    // Design coordinates assume scale 1 (SD). Scale them with s so a larger
    // UI grows as a whole; default s=1 keeps the classic SD size even in HD.

    // The attract mode: the country, the title, and how to play.
    if (hud.attract) {
        if (!hud.attract_text) return;
        if (!hud.banner.empty()) {
            text_center(fb, 48 * s, hud.banner, Value, 2 * s);
            text_center(fb, 65 * s, hud.banner_sub, Label, s);
        }
        // Above the road, clear of the followed car.
        text_center(fb, 84 * s, "KURVENRAUSCH", Label, 3 * s);
        if (hud.attract_prompt) text_center(fb, 112 * s, "PRESS ANY BUTTON", Value, 2 * s);
        text_center(fb, 130 * s, "OR TAP THE SCREEN", Value, s);
        return;
    }

    // Top left: current lap time.
    text(fb, 6 * s, 5 * s, "TIME", Label, s);
    text(fb, 6 * s, 14 * s, format_lap_time(hud.lap_time), Value, 2 * s);

    // Below it the lap counter; the top centre holds the rear-view mirror.
    text(fb, 6 * s, 33 * s, "LAP", Label, s);
    text(fb, 6 * s, 42 * s, hud.lap > 0 ? std::to_string(hud.lap) : "-", Value, 2 * s);
    if (!hud.time_of_day.empty()) text(fb, 34 * s, 49 * s, hud.time_of_day, Value, s); // the clock
    if (!hud.taxi.empty()) text(fb, 6 * s, 62 * s, hud.taxi, Label, s);

    // Top right: last and best laps.
    text_right(fb, w - 6 * s, 5 * s, "BEST", Label, s);
    text_right(fb, w - 6 * s, 14 * s, hud.best_lap > 0.f ? format_lap_time(hud.best_lap) : "-'--\"--", Value, s);
    text_right(fb, w - 6 * s, 25 * s, "LAST", Label, s);
    text_right(fb, w - 6 * s, 34 * s, hud.last_lap > 0.f ? format_lap_time(hud.last_lap) : "-'--\"--", Value, s);

    // Below them, the mini map.
    if (hud.map) draw_minimap(fb, w - 6 * s - 58 * s, 46 * s, 58 * s, hud);

    // At a lot with a choice: what is on offer, for a car with bars for its
    // strengths.
    int message_y = h / 2 - 42 * s;
    if (!hud.offer_title.empty()) {
        const int pw = 180 * s, px = (w - pw) / 2, py = 68 * s, ph = (hud.offer_stats ? 70 : 36) * s;
        message_y = py + ph + 6 * s; // below the panel, not over it
        for (int y = py; y < py + ph; ++y)
            for (int x = px; x < px + pw; ++x) fb.blend_pixel(x, y, Shadow, 0.7f);
        text_center(fb, py + 5 * s, hud.offer_title, Label, s);
        text_center(fb, py + 16 * s, "< " + hud.offer_name + " >", Value, 2 * s);
        const char* labels[3] = {"SPEED", "ACCEL", "GRIP"};
        for (int i = 0; hud.offer_stats && i < 3; ++i) {
            const int y = py + 36 * s + i * 10 * s;
            text(fb, px + 8 * s, y, labels[i], Label, s);
            const int bar = static_cast<int>(std::lround(std::clamp((hud.offer_values[i] - 0.6f) / 0.75f, 0.f, 1.f) * 90.f * s));
            fb.fill_rect(px + 50 * s, y, 90 * s, 7 * s, Color{0x30, 0x30, 0x40});
            fb.fill_rect(px + 50 * s, y, bar, 7 * s, Color{0x30, 0xe0, 0x40});
        }
    }

    // A fork ahead: which way goes where.
    if (!hud.fork_left.empty()) {
        text(fb, 6 * s, 112 * s, "< " + hud.fork_left, Value, s);
        text_right(fb, w - 6 * s, 112 * s, hud.fork_right + " >", Value, s);
    }

    // Bottom left: speed and revs.
    const int kmh = static_cast<int>(std::lround(hud.speed_kmh_fraction * top_speed_kmh));
    text_right(fb, 52 * s, h - 28 * s, std::to_string(kmh), Value, 3 * s);
    text(fb, 56 * s, h - 14 * s, "KM/H", Label, s);
    if (hud.reverse) text(fb, 86 * s, h - 21 * s, "R", Value, 2 * s);
    draw_tacho(fb, 6 * s, h - 37 * s, hud.speed_fraction, s);
    draw_lamps(fb, 6 * s, h - 47 * s, hud, s);
    draw_fuel(fb, 56 * s, h - 28 * s, hud.fuel, hud.fuel_warning, s);

    // Bottom right: nitro.
    if (hud.nitro_capacity > 0) {
        text_right(fb, w - 6 * s, h - 37 * s, "NITRO", Label, s);
        draw_nitro(fb, w - 6 * s, h - 27 * s, hud.nitro_capacity, hud.nitro, hud.nitro_burn, s);
    }

    if (hud.muted) text_right(fb, w - 6 * s, h - 9 * s, "MUTE", Label, s);

    // Chased: under the mirror, the police light and how near the car is to
    // getting away.
    if (hud.chase) {
        const int bar_w = 50 * s;
        const int lw = font::text_width("POLICE", s), x = (w - lw - 4 * s - bar_w) / 2, y = 41 * s;
        text(fb, x, y, "POLICE", hud.chase_red ? Color{0xff, 0x40, 0x30} : Color{0x40, 0x80, 0xff}, s);
        const int bx = x + lw + 4 * s;
        fb.fill_rect(bx - 1 * s, y, bar_w + 2 * s, 7 * s, Shadow);
        const int filled = static_cast<int>(std::lround(std::clamp(hud.escape, 0.f, 1.f) * static_cast<float>(bar_w)));
        if (filled > 0) fb.fill_rect(bx, y + 1 * s, filled, 5 * s, Color{0x30, 0xe0, 0x40});
    }

    if (!hud.banner.empty()) {
        text_center(fb, 48 * s, hud.banner, Value, 2 * s);
        text_center(fb, 65 * s, hud.banner_sub, Label, s);
    }

    if (!hud.message.empty() && hud.message_visible) {
        text_center(fb, message_y, hud.message, Label, 3 * s);
    }
}

void draw_menu(Canvas& fb, const MenuPage& page, const MenuView& view, bool capturing, bool blink,
               int ui_scale) {
    constexpr Color Dim{0xa0, 0xa8, 0xb8};
    constexpr Color Off{0x5c, 0x60, 0x70};
    constexpr Color Section{0x70, 0xb8, 0xf0};
    constexpr Color Frame{0x50, 0x60, 0x90};
    const MenuLayout l = menu_layout(page, fb.width(), fb.height(), ui_scale);
    const int s = l.scale;
    const auto tw = [s](std::string_view t) { return font::text_width(t, s); };

    dim_menu_bg(fb);
    fb.blend_rect(l.panel_x, l.panel_y, l.panel_w, l.panel_h, Color{0x06, 0x08, 0x18}, 0.7f);
    fb.fill_rect(l.panel_x, l.panel_y, l.panel_w, s, Frame);
    fb.fill_rect(l.panel_x, l.panel_y + l.panel_h - s, l.panel_w, s, Frame);
    fb.fill_rect(l.panel_x, l.panel_y, s, l.panel_h, Frame);
    fb.fill_rect(l.panel_x + l.panel_w - s, l.panel_y, s, l.panel_h, Frame);

    const int title_w = font::text_width(page.title, l.title_scale);
    text(fb, l.panel_x + (l.panel_w - title_w) / 2, l.title_y, page.title, Value, l.title_scale);

    const int n = static_cast<int>(page.items.size());
    const int text_dy = (l.row_h - font::glyph_h * s + 1) / 2;
    for (int r = 0; r < l.rows; ++r) {
        const int i = view.scroll + r;
        if (i >= n) break;
        const MenuItem& it = page.items[static_cast<size_t>(i)];
        const int y = l.list_y + r * l.row_h, ty = y + text_dy;
        const bool on = i == view.selected;
        const Color c = !it.enabled ? Off : on ? Label : Value;
        if (on) fb.blend_rect(l.panel_x + 2 * s, y, l.right - l.panel_x + 6 * s, l.row_h, Label, 0.16f);
        switch (it.kind) {
            case ItemKind::Heading: {
                const int w = tw(it.label), cx = (l.left + l.right) / 2, ly = y + l.row_h / 2;
                text(fb, cx - w / 2, ty, it.label, Section, s);
                fb.fill_rect(l.left, ly, cx - w / 2 - 6 * s - l.left, s, Frame);
                fb.fill_rect(cx + w / 2 + 6 * s, ly, l.right - (cx + w / 2 + 6 * s), s, Frame);
                break;
            }
            case ItemKind::Info:
                text(fb, l.left, ty, it.label, Dim, s);
                text_right(fb, l.right, ty, it.value, Value, s);
                break;
            case ItemKind::Action:
            case ItemKind::Submenu:
                if (l.centred) {
                    text(fb, (l.left + l.right - tw(it.label)) / 2, ty, it.label, c, s);
                } else {
                    text(fb, l.left, ty, it.label, c, s);
                    if (it.kind == ItemKind::Submenu) text_right(fb, l.right, ty, ">", on ? Label : Dim, s);
                }
                break;
            case ItemKind::Choice:
                text(fb, l.left, ty, it.label, c, s);
                if (on) {
                    const int vw = tw(it.value), arrow = tw("< ");
                    text_right(fb, l.right, ty, ">", Label, s);
                    text_right(fb, l.right - arrow, ty, it.value, Value, s);
                    text(fb, l.right - arrow - vw - arrow, ty, "<", Label, s);
                } else {
                    text_right(fb, l.right, ty, it.value, it.enabled ? Dim : Off, s);
                }
                break;
            case ItemKind::Slider: {
                text(fb, l.left, ty, it.label, c, s);
                const int cell = 4 * s, bar_x = l.right - it.levels * cell + s;
                for (int k = 0; k < it.levels; ++k) {
                    const bool lit = k < it.level;
                    fb.fill_rect(bar_x + k * cell + s, ty + s, cell - s, font::glyph_h * s, Shadow);
                    fb.fill_rect(bar_x + k * cell, ty, cell - s, font::glyph_h * s,
                                 lit ? (on ? Label : Value) : Color{0x30, 0x34, 0x48});
                }
                text_right(fb, bar_x - 4 * s, ty, std::to_string(it.level), on ? Label : Dim, s);
                break;
            }
            case ItemKind::Binding:
                text(fb, l.left, ty, it.label, c, s);
                for (int k = 0; k < 2; ++k) {
                    const bool picked = on && view.slot == k;
                    std::string v = k == 0 ? it.value : it.value2;
                    if (picked && capturing) v = blink ? "PRESS..." : "";
                    if (picked)
                        fb.blend_rect(l.slot_x[k] - 2 * s, y + s, l.slot_w + 4 * s, l.row_h - 2 * s, Label, 0.3f);
                    const Color vc = picked ? Label : v == "-" ? Off : Dim;
                    text(fb, l.slot_x[k] + (l.slot_w - tw(v)) / 2, ty, v, vc, s);
                }
                break;
        }
    }

    // The scroll bar, when there is more than fits.
    if (n > l.rows) {
        const int track_h = l.rows * l.row_h, x = l.right + 3 * s;
        fb.fill_rect(x, l.list_y, 2 * s, track_h, Color{0x30, 0x34, 0x48});
        const int thumb_h = std::max(4 * s, track_h * l.rows / n);
        const int thumb_y = l.list_y + (track_h - thumb_h) * view.scroll / std::max(1, n - l.rows);
        fb.fill_rect(x, thumb_y, 2 * s, thumb_h, Dim);
    }

    std::string_view hint = page.hint;
    if (view.selected >= 0 && view.selected < n && !page.items[static_cast<size_t>(view.selected)].help.empty())
        hint = page.items[static_cast<size_t>(view.selected)].help;
    text(fb, l.panel_x + (l.panel_w - tw(hint)) / 2, l.hint_y, hint, Dim, s);
}

void draw_fps(Canvas& fb, float fps, const frame_stats::Snapshot& stats) {
    const Color col{0xe0, 0xe8, 0x40};
    char buf[96];
    std::snprintf(buf, sizeof buf, "%.0f FPS  %.1fms%s", static_cast<double>(fps), stats.ms_frame,
                  frame_stats::gpu_sync() ? "  GPU SYNC" : "");
    fb.draw_text(4, 4, buf, col);
    if (stats.ms_frame <= 0.0 && stats.draw_calls == 0) return;
    // Second line: draws + vertex totals.
    std::snprintf(buf, sizeof buf, "draws %d  solid %dk  tex %dk", stats.draw_calls,
                  stats.solid_verts / 1000, stats.tex_verts / 1000);
    fb.draw_text(4, 14, buf, col);
    // Third: scene counts when GLES.
    if (stats.gles) {
        std::snprintf(buf, sizeof buf, "slices %d  spr %d  lamps %d", stats.slices, stats.sprites, stats.lamps);
        fb.draw_text(4, 24, buf, col);
    }
    // Phase times — only show phases that took ≥0.1ms, sorted by cost.
    struct Row {
        const char* name;
        double ms;
        int phase;
    };
    Row rows[static_cast<int>(frame_stats::Phase::Count)];
    int n = 0;
    for (int i = 0; i < static_cast<int>(frame_stats::Phase::Count); ++i) {
        if (stats.ms[i] < 0.1) continue;
        rows[n++] = {frame_stats::phase_name(static_cast<frame_stats::Phase>(i)), stats.ms[i], i};
    }
    for (int i = 0; i < n; ++i)
        for (int j = i + 1; j < n; ++j)
            if (rows[j].ms > rows[i].ms) std::swap(rows[i], rows[j]);
    int y = stats.gles ? 34 : 24;
    for (int i = 0; i < n && i < 6; ++i) {
        const double screens = stats.screen_px > 0 ? stats.fill[rows[i].phase] / stats.screen_px : 0.0;
        if (screens >= 0.05)
            std::snprintf(buf, sizeof buf, "%-7s %5.1fms %4.1fx", rows[i].name, rows[i].ms, screens);
        else
            std::snprintf(buf, sizeof buf, "%-7s %5.1fms", rows[i].name, rows[i].ms);
        fb.draw_text(4, y, buf, col);
        y += 10;
    }
}

} // namespace racer
