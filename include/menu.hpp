// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <string>
#include <vector>

namespace racer {

// One tick of menu navigation: presses (and key repeats), not held keys.
struct MenuInput {
    bool up = false;
    bool down = false;
    bool left = false;
    bool right = false;
    bool confirm = false;
    bool back = false;
    bool clear = false;     // Delete, or X on a pad: unbind on the CONTROLS pages
    bool page_up = false;   // Page Up, or the left shoulder
    bool page_down = false; // Page Down, or the right shoulder
    int scroll = 0;         // mouse wheel notches, + down
};

// The pause menu is a stack of pages, each a list of these. The game builds
// a page from its state whenever it is needed (see game_menu.cpp), so what
// is shown is always what is in effect; MenuView remembers where on a page
// the player is.
enum class ItemKind {
    Action,  // confirm does it
    Submenu, // confirm opens another page
    Choice,  // left / right (and confirm) step through values
    Slider,  // left / right a level, 0 .. levels
    Binding, // two slots side by side, left / right picks one, confirm rebinds it
    Heading, // a section title, not selectable
    Info,    // a line of text, label and value, not selectable
};

struct MenuItem {
    int id = 0;
    ItemKind kind = ItemKind::Action;
    std::string label;
    std::string value;  // Choice, Info: the value shown; Binding: the first slot
    std::string value2; // Binding: the second slot
    int level = 0;      // Slider
    int levels = 0;
    bool enabled = true; // false: greyed out and skipped
    std::string help;    // shown at the bottom while selected
};

bool selectable(const MenuItem& item);

struct MenuPage {
    std::string title;
    std::vector<MenuItem> items;
    std::string hint; // the bottom line when the selected item has no help
    bool big_title = false;
};

// What the player did on a page this tick.
struct MenuEvent {
    enum class Type { None, Activate, Change, Clear, Back };
    Type type = Type::None;
    int id = -1;  // the item's id
    int step = 0; // Change: -1 or +1
    int slot = 0; // Binding: which slot
};

// Where things are on screen for a page, in framebuffer pixels. Scale is the
// UI scale (1 = SD design size, independent of framebuffer HD); the panel is
// as wide as its widest line needs and centred, the list as long as fits,
// scrolling when there is more.
struct MenuLayout {
    int scale = 1;
    int panel_x = 0, panel_y = 0, panel_w = 0, panel_h = 0;
    int title_y = 0;
    int title_scale = 2;
    int list_y = 0;
    int row_h = 12;
    int rows = 0;        // rows shown at once
    int left = 0;        // labels start here
    int right = 0;       // values end here
    int slot_w = 0;      // Binding: width of a slot column
    int slot_x[2] = {0, 0};
    bool centred = false; // a page of only actions and submenus: lines centred
    int hint_y = 0;
};
// ui_scale: 1..3 design-pixel scale for the panel (not tied to HD resolution).
MenuLayout menu_layout(const MenuPage& page, int width, int height, int ui_scale = 1);

struct MenuView {
    int selected = -1; // item index; -1: the first selectable one
    int scroll = 0;    // first row shown
    int slot = 0;      // Binding: the slot picked

    void reset() { *this = MenuView{}; }
    // Keeps the selection on a selectable item and within the rows shown.
    void settle(const MenuPage& page, int rows);
    MenuEvent update(const MenuPage& page, const MenuInput& in, int rows);
    // A tap or click at (x, y): selects what is there and acts as a press
    // would; on a Choice or Slider the half of the line tapped picks the way.
    MenuEvent click(const MenuPage& page, const MenuLayout& layout, float x, float y);
    // The pointer moved to (x, y): selects what is under it.
    void hover(const MenuPage& page, const MenuLayout& layout, float x, float y);

private:
    void move(const MenuPage& page, int dir, bool wrap);
    int item_at(const MenuPage& page, const MenuLayout& layout, float x, float y) const;
};

} // namespace racer
