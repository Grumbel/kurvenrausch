// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "synth.hpp"

#include <SDL2/SDL.h>

namespace racer {

// Plays a Synth through the default SDL audio device. The synth must outlive
// the Audio object. Having no audio device is not an error for the game: it
// simply stays silent.
class Audio {
public:
    Audio() = default;
    ~Audio();
    Audio(const Audio&) = delete;
    Audio& operator=(const Audio&) = delete;

    bool init(Synth& synth);
    bool active() const { return device_ != 0; }

private:
    static void callback(void* userdata, Uint8* stream, int length);

    SDL_AudioDeviceID device_ = 0;
};

} // namespace racer
