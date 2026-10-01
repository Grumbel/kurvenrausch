// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

namespace racer {

struct InputState {
    // Held keys
    bool up = false;
    bool down = false;
    bool left = false;
    bool right = false;
    // One-shot events
    bool quit = false;
    bool restart = false;
    bool toggle_fullscreen = false;
};

class Input {
public:
    // Polls SDL events. Held keys are refreshed, one-shot events are set when
    // they occurred since the last poll.
    void poll(InputState& state);
};

} // namespace racer
