// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "touch.hpp"

#include <algorithm>
#include <cmath>
#include <string_view>

namespace racer {

namespace {

bool inside(const TouchControls::Circle& c, float x, float y) {
    const float dx = x - c.x, dy = y - c.y;
    return dx * dx + dy * dy <= c.r * c.r;
}

// A translucent disc with a rim, brighter while pressed.
void disc(Framebuffer& fb, const TouchControls::Circle& c, bool pressed) {
    const int r = static_cast<int>(c.r);
    const Color fill = pressed ? Color{0xff, 0xd8, 0x30} : Color{0x20, 0x20, 0x30};
    for (int dy = -r; dy <= r; ++dy) {
        for (int dx = -r; dx <= r; ++dx) {
            const float d = std::sqrt(static_cast<float>(dx * dx + dy * dy));
            if (d > c.r) continue;
            const int x = static_cast<int>(c.x) + dx, y = static_cast<int>(c.y) + dy;
            if (d > c.r - 1.5f) fb.blend_pixel(x, y, Color{0xf8, 0xf8, 0xf8}, 0.6f);
            else fb.blend_pixel(x, y, fill, pressed ? 0.45f : 0.35f);
        }
    }
}

void label(Framebuffer& fb, const TouchControls::Circle& c, std::string_view text) {
    const int w = static_cast<int>(text.size()) * 6 - 1;
    fb.draw_text(static_cast<int>(c.x) - w / 2 + 1, static_cast<int>(c.y) - 2, text, Color{0x10, 0x10, 0x20});
    fb.draw_text(static_cast<int>(c.x) - w / 2, static_cast<int>(c.y) - 3, text, Color{0xf8, 0xf8, 0xf8});
}

} // namespace

TouchControls::Circle TouchControls::circle(Button b) {
    switch (b) {
        case Gas: return {288.f, 168.f, 24.f};
        case Brake: return {238.f, 190.f, 20.f};
        case Nitro: return {292.f, 120.f, 15.f};
        case Handbrake: return {196.f, 206.f, 14.f};
        case Horn: return {248.f, 142.f, 12.f};
        default: return {236.f, 22.f, 10.f}; // Pause: between the mirror and BEST
    }
}

TouchInput TouchControls::update(const std::vector<Finger>& fingers) {
    TouchInput in;
    bool held[buttons] = {};
    std::map<int64_t, Finger> steering;
    for (const Finger& f : fingers) {
        bool on_button = false;
        for (int b = 0; b < buttons; ++b) {
            if (inside(circle(static_cast<Button>(b)), f.x, f.y)) {
                held[b] = true;
                on_button = true;
            }
        }
        // A finger that started steering keeps steering wherever it goes.
        const bool steering_already = steer_origin_.count(f.id) > 0;
        if (steering_already || (!on_button && f.x < steer_zone_right && f.y > steer_zone_top)) {
            if (!steering_already) steer_origin_[f.id] = f.x;
            steering[f.id] = f;
        }
    }
    for (auto it = steer_origin_.begin(); it != steer_origin_.end();) {
        it = steering.count(it->first) ? std::next(it) : steer_origin_.erase(it);
    }
    // The newest steering finger wins.
    steer_ = 0.f;
    for (const auto& [id, f] : steering) {
        steer_ = std::clamp((f.x - steer_origin_[id]) / steer_travel, -1.f, 1.f);
    }
    steering_ = std::move(steering);

    in.steer = steer_;
    in.throttle = held[Gas] ? 1.f : 0.f;
    in.brake = held[Brake] ? 1.f : 0.f;
    in.nitro = held[Nitro];
    in.handbrake = held[Handbrake];
    in.horn = held[Horn];
    in.pause = held[Pause] && !held_[Pause];
    std::copy(std::begin(held), std::end(held), std::begin(held_));
    return in;
}

void TouchControls::release() {
    steer_origin_.clear();
    steering_.clear();
    std::fill(std::begin(held_), std::end(held_), false);
    steer_ = 0.f;
}

void TouchControls::draw(Framebuffer& fb) const {
    static constexpr std::string_view names[buttons] = {"GAS", "BRK", "NOS", "HB", "H", ""};
    for (int b = 0; b < buttons; ++b) {
        const Circle c = circle(static_cast<Button>(b));
        disc(fb, c, held_[b]);
        if (b == Pause) {
            fb.fill_rect(static_cast<int>(c.x) - 3, static_cast<int>(c.y) - 4, 2, 8, Color{0xf8, 0xf8, 0xf8});
            fb.fill_rect(static_cast<int>(c.x) + 2, static_cast<int>(c.y) - 4, 2, 8, Color{0xf8, 0xf8, 0xf8});
        } else {
            label(fb, c, names[b]);
        }
    }
    // The steering pad: arrows where nobody steers, else a ring where the
    // finger came down and a knob as far as it has steered.
    if (steering_.empty()) {
        const Circle l{40.f, 170.f, 12.f}, r{88.f, 170.f, 12.f};
        disc(fb, l, false);
        disc(fb, r, false);
        label(fb, l, "<");
        label(fb, r, ">");
        return;
    }
    for (const auto& [id, f] : steering_) {
        const float origin = steer_origin_.at(id);
        disc(fb, {origin, f.y, steer_travel}, false);
        disc(fb, {origin + steer_ * steer_travel, f.y, 10.f}, true);
    }
}

MenuTap menu_tap(const PauseMenu& menu, float x, float y, int fb_width, int fb_height) {
    // The lines as draw_pause_menu() lays them out.
    const float first = static_cast<float>(fb_height / 2 - 50 + 36);
    for (int i = 0; i < menu.item_count(); ++i) {
        const float line = first + 16.f * static_cast<float>(i) + 3.f;
        if (std::abs(y - line) > 8.f) continue;
        const float third = static_cast<float>(fb_width) / 3.f;
        const int side = i != PauseMenu::StartZone ? 0 : x < third ? -1 : x > 2.f * third ? 1 : 0;
        return {i, side};
    }
    return {};
}

} // namespace racer
