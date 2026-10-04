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

} // namespace

float TouchControls::unit() const {
    return std::min(layout_.screen_w / 320.f, layout_.screen_h / 240.f);
}

TouchControls::Circle TouchControls::circle(Button b) const {
    // The pedals and buttons keep to the screen's right edge.
    const float u = unit();
    const auto right = [&](float x, float y, float r) { return Circle{layout_.screen_w - (320.f - x) * u, y * u, r * u}; };
    switch (b) {
        case Gas: return right(288.f, 168.f, 24.f);
        case Brake: return right(238.f, 190.f, 20.f);
        case Nitro: return right(292.f, 120.f, 15.f);
        case Handbrake: return right(196.f, 206.f, 14.f);
        case Horn: return right(248.f, 142.f, 12.f);
        default: {
            // Pause: on the picture, between the mirror (centred) and BEST.
            const float g = layout_.game_h / 240.f;
            return {layout_.game_x + layout_.game_w / 2.f + 76.f * g, layout_.game_y + 22.f * g, 10.f * g};
        }
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
        if (steering_already || (!on_button && f.x < steer_zone_right() && f.y > steer_zone_top())) {
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
        steer_ = std::clamp((f.x - steer_origin_[id]) / steer_travel(), -1.f, 1.f);
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

void TouchControls::draw(Overlay& overlay) const {
    static constexpr std::string_view names[buttons] = {"GAS", "BRK", "NOS", "HB", "H", ""};
    const float u = unit();
    const float rim = 1.5f * u;
    const int text_scale = std::max(1, static_cast<int>(std::lround(u)));
    const Color white{0xf8, 0xf8, 0xf8};
    for (int b = 0; b < buttons; ++b) {
        const Circle c = circle(static_cast<Button>(b));
        overlay.disc(c.x, c.y, c.r, rim, held_[b]);
        if (b == Pause) {
            const float g = c.r / 10.f;
            overlay.rect(c.x - 3.f * g, c.y - 4.f * g, 2.f * g, 8.f * g, white);
            overlay.rect(c.x + 2.f * g, c.y - 4.f * g, 2.f * g, 8.f * g, white);
        } else {
            overlay.text(c.x, c.y, names[b], text_scale);
        }
    }
    // The steering pad: arrows where nobody steers, else a ring where the
    // finger came down and a knob as far as it has steered.
    if (steering_.empty()) {
        const Circle l{40.f * u, 170.f * u, 12.f * u}, r{88.f * u, 170.f * u, 12.f * u};
        overlay.disc(l.x, l.y, l.r, rim, false);
        overlay.disc(r.x, r.y, r.r, rim, false);
        overlay.text(l.x, l.y, "<", text_scale);
        overlay.text(r.x, r.y, ">", text_scale);
        return;
    }
    for (const auto& [id, f] : steering_) {
        const float origin = steer_origin_.at(id);
        overlay.disc(origin, f.y, steer_travel(), rim, false);
        overlay.disc(origin + steer_ * steer_travel(), f.y, 10.f * u, rim, true);
    }
}

MenuTap menu_tap(const PauseMenu& menu, float x, float y, int fb_width, int fb_height) {
    // The lines as draw_pause_menu() lays them out.
    const float first = static_cast<float>(fb_height / 2 - 50 + 36);
    for (int i = 0; i < menu.item_count(); ++i) {
        const float line = first + 16.f * static_cast<float>(i) + 3.f;
        if (std::abs(y - line) > 8.f) continue;
        const float third = static_cast<float>(fb_width) / 3.f;
        const bool sides = i == PauseMenu::StartZone || i == PauseMenu::Track;
        const int side = !sides ? 0 : x < third ? -1 : x > 2.f * third ? 1 : 0;
        return {i, side};
    }
    return {};
}

MenuTap options_tap(float x, float y, int fb_width, int fb_height) {
    const float first = static_cast<float>(fb_height / 2 - 50 + 36);
    for (int i = 0; i < OptionsMenu::items; ++i) {
        const float line = first + 16.f * static_cast<float>(i) + 3.f;
        if (std::abs(y - line) > 8.f) continue;
        return {i, x < static_cast<float>(fb_width) / 3.f ? -1 : 1};
    }
    return {};
}

} // namespace racer
