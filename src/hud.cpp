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

    // Bottom left: speed and revs.
    const int kmh = static_cast<int>(std::lround(hud.speed_fraction * top_speed_kmh));
    text_right(fb, 52, h - 28, std::to_string(kmh), Value, 3);
    text(fb, 56, h - 14, "KM/H", Label);
    draw_tacho(fb, 6, h - 37, hud.speed_fraction);

    // Bottom right: nitro.
    text_right(fb, w - 6, h - 37, "NITRO", Label);
    draw_nitro(fb, w - 6, h - 27, hud.nitro, hud.nitro_burn);

    if (hud.muted) text_right(fb, w - 6, h - 9, "MUTE", Label);

    if (!hud.banner.empty()) {
        text_center(fb, 48, hud.banner, Value, 2);
        text_center(fb, 65, hud.banner_sub, Label);
    }

    if (!hud.message.empty() && hud.message_visible) {
        text_center(fb, h / 2 - 42, hud.message, Label, 3);
    }
}

} // namespace racer
