// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "audio.hpp"

#include <iostream>

namespace racer {

Audio::~Audio() {
    if (device_) SDL_CloseAudioDevice(device_);
    if (SDL_WasInit(SDL_INIT_AUDIO)) SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

void Audio::callback(void* userdata, Uint8* stream, int length) {
    // Mono, signed 16 bit: two bytes per sample.
    static_cast<Synth*>(userdata)->render(reinterpret_cast<int16_t*>(stream), length / 2);
}

bool Audio::init(Synth& synth) {
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
        std::cerr << "Audio unavailable: " << SDL_GetError() << "\n";
        return false;
    }

    SDL_AudioSpec want;
    SDL_zero(want);
    want.freq = Synth::sample_rate;
    want.format = AUDIO_S16SYS;
    want.channels = 1;
#ifdef __EMSCRIPTEN__
    want.samples = 2048; // the browser's audio needs a larger buffer to play without gaps
#else
    want.samples = 512; // about 12 ms of latency
#endif
    want.callback = &Audio::callback;
    want.userdata = &synth;

    // Allow no changes: SDL converts to the device's format for us, so the
    // synth always renders what it expects.
    SDL_AudioSpec have;
    device_ = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
    if (device_ == 0) {
        std::cerr << "Could not open an audio device: " << SDL_GetError() << "\n";
        return false;
    }
    SDL_PauseAudioDevice(device_, 0);
    return true;
}

} // namespace racer
