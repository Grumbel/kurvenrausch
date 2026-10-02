// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>

namespace racer {

// The car radio: a few synthesised songs, each a four-bar loop of pad
// chords, a bass line, a lead melody and drums, played by a sixteenth-note
// step sequencer. Deterministic: the output depends only on the song and
// the samples played so far. Runs on the audio thread (inside Synth).
class Music {
public:
    static constexpr int tracks = 3;
    // A track's name, or "RADIO OFF" for -1.
    static const char* name(int track);
    // Next and previous in the order of the dial: the songs, then off.
    static int next(int track) { return track + 1 >= tracks ? -1 : track + 1; }
    static int previous(int track) { return track < 0 ? tracks - 1 : track - 1; }

    // Starts a song from its beginning; -1 stops.
    void select(int track);
    int track() const { return track_; }
    // One sample, about -1 .. 1.
    float sample();

private:
    float noise();
    void step();

    int track_ = -1;
    uint64_t pos_ = 0;     // samples into the song
    int step_ = -1;        // the sixteenth playing
    uint32_t rng_ = 0x1badf00du;
    // Voices: phases, envelopes, filters.
    double bass_phase_ = 0.0, lead_phase_ = 0.0, pad_phase_[3] = {0.0, 0.0, 0.0}, kick_phase_ = 0.0, vibrato_ = 0.0;
    float bass_freq_ = 0.f, lead_freq_ = 0.f, pad_freq_[3] = {0.f, 0.f, 0.f};
    float bass_env_ = 0.f, lead_env_ = 0.f, lead_age_ = 0.f, kick_env_ = 0.f, kick_age_ = 0.f, snare_env_ = 0.f,
          hat_env_ = 0.f;
    float bass_lp_ = 0.f, lead_lp_ = 0.f, pad_lp_ = 0.f, snare_lp_ = 0.f, hat_lp_ = 0.f;
};

} // namespace racer
