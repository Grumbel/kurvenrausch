// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "menu.hpp"

#include "font.hpp"

#include <algorithm>

namespace racer {

namespace {

constexpr int scrollbar_w = 6; // at scale 1, including its gap

bool has_values(const MenuPage& page) {
    return std::any_of(page.items.begin(), page.items.end(), [](const MenuItem& i) {
        return i.kind == ItemKind::Choice || i.kind == ItemKind::Slider || i.kind == ItemKind::Binding ||
               i.kind == ItemKind::Info;
    });
}

} // namespace

bool selectable(const MenuItem& item) {
    return item.enabled && item.kind != ItemKind::Heading && item.kind != ItemKind::Info;
}

MenuLayout menu_layout(const MenuPage& page, int width, int height) {
    MenuLayout l;
    const int s = std::max(1, height / 240);
    l.scale = s;
    l.row_h = 12 * s;
    l.title_scale = (page.big_title ? 3 : 2) * s;
    l.centred = !has_values(page);
    const auto tw = [s](const std::string& t) { return font::text_width(t, s); };

    const int margin = 6 * s, vpad = 6 * s, pad = 10 * s;
    const int title_h = font::glyph_h * l.title_scale, title_gap = 8 * s;
    const int hint_h = font::glyph_h * s, hint_gap = 7 * s;
    const int n = static_cast<int>(page.items.size());
    const int avail = height - 2 * margin - 2 * vpad - title_h - title_gap - hint_gap - hint_h;
    l.rows = std::clamp(avail / l.row_h, 1, std::max(1, n));
    const bool scrolling = n > l.rows;

    // As wide as the widest line needs.
    int label_w = 0, value_w = 0, slot_w = tw("PRESS..."), wide = 150 * s;
    bool bindings = false;
    for (const MenuItem& it : page.items) {
        const int lw = tw(it.label);
        switch (it.kind) {
            case ItemKind::Heading: wide = std::max(wide, lw + 32 * s); continue;
            case ItemKind::Choice: value_w = std::max(value_w, tw("< " + it.value + " >")); break;
            case ItemKind::Slider: value_w = std::max(value_w, tw("10 ") + it.levels * 4 * s); break;
            case ItemKind::Info: value_w = std::max(value_w, tw(it.value)); break;
            case ItemKind::Submenu: value_w = std::max(value_w, tw(">")); break;
            case ItemKind::Binding:
                bindings = true;
                slot_w = std::max({slot_w, tw(it.value), tw(it.value2)});
                break;
            default: break;
        }
        label_w = std::max(label_w, lw);
        wide = std::max(wide, tw(it.help));
    }
    wide = std::max({wide, tw(page.hint), font::text_width(page.title, l.title_scale)});
    if (l.centred) wide = std::max(wide, label_w);
    else if (bindings) wide = std::max(wide, label_w + 8 * s + 2 * slot_w + 6 * s);
    else wide = std::max(wide, label_w + 12 * s + value_w);

    const int bar = scrolling ? scrollbar_w * s : 0;
    l.panel_w = std::min(width - 2 * margin, wide + 2 * pad + bar);
    l.panel_x = (width - l.panel_w) / 2;
    l.left = l.panel_x + pad;
    l.right = l.panel_x + l.panel_w - pad - bar;
    l.slot_w = slot_w;
    l.slot_x[1] = l.right - slot_w;
    l.slot_x[0] = l.slot_x[1] - 6 * s - slot_w;

    l.panel_h = vpad + title_h + title_gap + l.rows * l.row_h + hint_gap + hint_h + vpad;
    l.panel_y = (height - l.panel_h) / 2;
    l.title_y = l.panel_y + vpad;
    l.list_y = l.title_y + title_h + title_gap;
    l.hint_y = l.list_y + l.rows * l.row_h + hint_gap;
    return l;
}

void MenuView::settle(const MenuPage& page, int rows) {
    const int n = static_cast<int>(page.items.size());
    rows = std::max(1, rows);
    if (selected < 0 || selected >= n || !selectable(page.items[static_cast<size_t>(selected)])) {
        const int from = std::clamp(selected, 0, std::max(0, n - 1));
        int found = -1;
        for (int i = from; i < n && found < 0; ++i)
            if (selectable(page.items[static_cast<size_t>(i)])) found = i;
        for (int i = from - 1; i >= 0 && found < 0; --i)
            if (selectable(page.items[static_cast<size_t>(i)])) found = i;
        selected = found;
    }
    if (selected >= 0) {
        // A row of context above and below the selection where there is room.
        const int context = rows >= 5 ? 1 : 0;
        scroll = std::min(scroll, selected - context);
        scroll = std::max(scroll, selected + context - rows + 1);
    }
    scroll = std::clamp(scroll, 0, std::max(0, n - rows));
}

void MenuView::move(const MenuPage& page, int dir, bool wrap) {
    const int n = static_cast<int>(page.items.size());
    if (n == 0 || selected < 0) return;
    int i = selected;
    for (int k = 0; k < n; ++k) {
        i += dir;
        if (i < 0 || i >= n) {
            if (!wrap) return;
            i = (i + n) % n;
        }
        if (selectable(page.items[static_cast<size_t>(i)])) {
            if (i != selected) slot = 0;
            selected = i;
            return;
        }
    }
}

MenuEvent MenuView::update(const MenuPage& page, const MenuInput& in, int rows) {
    settle(page, rows);
    MenuEvent ev;
    if (in.back) {
        ev.type = MenuEvent::Type::Back;
        return ev;
    }
    if (in.up) move(page, -1, true);
    if (in.down) move(page, 1, true);
    for (int k = 0; k < rows - 1; ++k) {
        if (in.page_up) move(page, -1, false);
        if (in.page_down) move(page, 1, false);
    }
    for (int k = 0; k < std::abs(in.scroll); ++k) move(page, in.scroll < 0 ? -1 : 1, false);
    settle(page, rows);
    if (selected < 0) return ev;

    const MenuItem& it = page.items[static_cast<size_t>(selected)];
    ev.id = it.id;
    switch (it.kind) {
        case ItemKind::Action:
        case ItemKind::Submenu:
            if (in.confirm) ev.type = MenuEvent::Type::Activate;
            break;
        case ItemKind::Choice:
            if (in.left) ev = MenuEvent{MenuEvent::Type::Change, it.id, -1, 0};
            else if (in.right || in.confirm) ev = MenuEvent{MenuEvent::Type::Change, it.id, 1, 0};
            break;
        case ItemKind::Slider:
            if (in.left) ev = MenuEvent{MenuEvent::Type::Change, it.id, -1, 0};
            else if (in.right) ev = MenuEvent{MenuEvent::Type::Change, it.id, 1, 0};
            break;
        case ItemKind::Binding:
            if (in.left) slot = 0;
            if (in.right) slot = 1;
            if (in.confirm) ev = MenuEvent{MenuEvent::Type::Activate, it.id, 0, slot};
            else if (in.clear) ev = MenuEvent{MenuEvent::Type::Clear, it.id, 0, slot};
            break;
        default: break;
    }
    return ev;
}

int MenuView::item_at(const MenuPage& page, const MenuLayout& l, float x, float y) const {
    if (x < static_cast<float>(l.panel_x) || x >= static_cast<float>(l.panel_x + l.panel_w)) return -1;
    if (y < static_cast<float>(l.list_y) || y >= static_cast<float>(l.list_y + l.rows * l.row_h)) return -1;
    const int i = scroll + static_cast<int>((y - static_cast<float>(l.list_y)) / static_cast<float>(l.row_h));
    return i < static_cast<int>(page.items.size()) ? i : -1;
}

MenuEvent MenuView::click(const MenuPage& page, const MenuLayout& l, float x, float y) {
    MenuEvent ev;
    const int n = static_cast<int>(page.items.size());
    // The scroll bar: a page up or down, whichever side of the thumb.
    if (n > l.rows && x >= static_cast<float>(l.right) && x < static_cast<float>(l.panel_x + l.panel_w) &&
        y >= static_cast<float>(l.list_y) && y < static_cast<float>(l.list_y + l.rows * l.row_h)) {
        MenuInput in;
        const float middle = static_cast<float>(l.list_y) + static_cast<float>(l.rows * l.row_h) *
                                                                (static_cast<float>(scroll) + l.rows / 2.f) /
                                                                static_cast<float>(n);
        (y < middle ? in.page_up : in.page_down) = true;
        update(page, in, l.rows);
        return ev;
    }
    const int i = item_at(page, l, x, y);
    if (i < 0 || !selectable(page.items[static_cast<size_t>(i)])) return ev;
    hover(page, l, x, y);
    const MenuItem& it = page.items[static_cast<size_t>(i)];
    ev.id = it.id;
    switch (it.kind) {
        case ItemKind::Action:
        case ItemKind::Submenu: ev.type = MenuEvent::Type::Activate; break;
        case ItemKind::Choice:
        case ItemKind::Slider:
            ev.type = MenuEvent::Type::Change;
            ev.step = x < static_cast<float>(l.panel_x) + static_cast<float>(l.panel_w) / 2.f ? -1 : 1;
            break;
        case ItemKind::Binding:
            ev.type = MenuEvent::Type::Activate;
            ev.slot = slot;
            break;
        default: break;
    }
    settle(page, l.rows);
    return ev;
}

void MenuView::hover(const MenuPage& page, const MenuLayout& l, float x, float y) {
    const int i = item_at(page, l, x, y);
    if (i < 0 || !selectable(page.items[static_cast<size_t>(i)])) return;
    if (i != selected) slot = 0;
    selected = i;
    if (page.items[static_cast<size_t>(i)].kind == ItemKind::Binding) {
        if (x >= static_cast<float>(l.slot_x[1])) slot = 1;
        else if (x >= static_cast<float>(l.slot_x[0])) slot = 0;
    }
}

} // namespace racer
