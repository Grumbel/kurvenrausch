// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

namespace racer {

class Display;
class Input;
class SpriteSheet;

// Interactive browser of every sprite the sheet builds. Left/right change
// the sprite, up/down the category; Esc, Start or B leaves. `run_sprite_viewer`
// opens its own window (CLI --sprites); the session form reuses the game's display.
bool run_sprite_viewer(bool fullscreen = false);
bool run_sprite_viewer_session(Display& display, Input& input, const SpriteSheet& sheet);

} // namespace racer
