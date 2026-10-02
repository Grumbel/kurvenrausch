// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "music.hpp"

#include "synth.hpp"

#include <cmath>

namespace racer {

namespace {

constexpr float sr = static_cast<float>(Synth::sample_rate);
constexpr double two_pi = 6.283185307179586;

struct Song {
    const char* name;
    float bpm;
    int chords[4][3];  // MIDI notes of the pad, one chord per bar
    int roots[4];      // the bass's root per bar
    int bass[8];       // per eighth: semitones over the root, -1 rests
    const char* drums; // per sixteenth: K kick, S snare, h hat, . nothing
    int lead[64];      // per sixteenth over the four bars: a MIDI note, 0 lets the last one ring
    float lead_bright; // 0 a soft square .. 1 a buzzy saw
};

const Song songs[Music::tracks] = {
    {"SUNSET CRUISE", 118.f,
     {{57, 60, 64}, {53, 57, 60}, {55, 60, 64}, {55, 59, 62}},
     {45, 41, 48, 43},
     {0, 0, 12, 0, 0, 0, 7, 0},
     "K.h.S.h.K.K.S.hh",
     {76, 0, 0, 74, 72, 0, 69, 0, 72, 0, 74, 0, 76, 0, 0, 0,
      77, 0, 0, 76, 74, 0, 72, 0, 69, 0, 72, 0, 74, 0, 0, 0,
      79, 0, 0, 76, 72, 0, 76, 0, 79, 0, 81, 0, 79, 0, 76, 0,
      74, 0, 0, 0, 71, 0, 74, 0, 79, 0, 0, 74, 76, 0, 0, 0},
     0.3f},
    {"TURBO BREEZE", 136.f,
     {{60, 64, 67}, {59, 62, 67}, {57, 60, 64}, {57, 60, 65}},
     {48, 43, 45, 41},
     {0, 12, 0, 12, 0, 12, 7, 12},
     "K.h.S.hKK.h.S.h.",
     {72, 0, 76, 0, 79, 0, 76, 0, 84, 0, 0, 79, 76, 0, 72, 0,
      74, 0, 79, 0, 83, 0, 79, 0, 86, 0, 0, 83, 79, 0, 74, 0,
      76, 0, 72, 0, 69, 0, 72, 0, 76, 0, 79, 0, 81, 0, 0, 0,
      77, 0, 76, 0, 74, 0, 72, 0, 74, 0, 0, 0, 79, 0, 0, 0},
     0.8f},
    {"NIGHT DRIVE", 96.f,
     {{62, 65, 69}, {58, 62, 65}, {57, 60, 65}, {55, 60, 64}},
     {38, 34, 41, 36},
     {0, 0, 0, 0, 0, 0, 0, 0},
     "K.h.S.h.K.hKS.h.",
     {74, 0, 0, 0, 0, 0, 77, 0, 76, 0, 0, 0, 74, 0, 72, 0,
      74, 0, 0, 0, 0, 0, 0, 0, 70, 0, 72, 0, 74, 0, 0, 0,
      72, 0, 0, 0, 0, 0, 77, 0, 81, 0, 0, 0, 79, 0, 77, 0,
      76, 0, 0, 0, 0, 0, 0, 0, 72, 0, 74, 0, 76, 0, 0, 0},
     0.5f},
};

float midi(int note) { return 440.f * std::pow(2.f, static_cast<float>(note - 69) / 12.f); }
float lowpass(float hz) { return 1.f - std::exp(-6.2831853f * hz / sr); }

} // namespace

const char* Music::name(int track) {
    return track >= 0 && track < tracks ? songs[track].name : "RADIO OFF";
}

void Music::select(int track) {
    *this = Music{};
    track_ = track >= 0 && track < tracks ? track : -1;
}

float Music::noise() {
    rng_ ^= rng_ << 13;
    rng_ ^= rng_ >> 17;
    rng_ ^= rng_ << 5;
    return static_cast<float>(rng_) / 2147483648.f - 1.f;
}

// A new sixteenth: start the notes and drums it holds.
void Music::step() {
    const Song& s = songs[track_];
    const int i = step_ % 64, bar = i / 16, in_bar = i % 16;
    if (in_bar == 0) {
        for (int k = 0; k < 3; ++k) pad_freq_[k] = midi(s.chords[bar][k]);
    }
    if (in_bar % 2 == 0) {
        const int b = s.bass[in_bar / 2];
        if (b >= 0) {
            bass_freq_ = midi(s.roots[bar] + b);
            bass_env_ = 1.f;
        }
    }
    if (s.lead[i] > 0) {
        lead_freq_ = midi(s.lead[i]);
        lead_env_ = 1.f;
        lead_age_ = 0.f;
    }
    switch (s.drums[in_bar]) {
        case 'K': kick_env_ = 1.f; kick_age_ = 0.f; kick_phase_ = 0.0; break;
        case 'S': snare_env_ = 1.f; break;
        case 'h': hat_env_ = 1.f; break;
        default: break;
    }
}

float Music::sample() {
    if (track_ < 0) return 0.f;
    const Song& s = songs[track_];
    const double samples_per_step = static_cast<double>(sr) * 60.0 / static_cast<double>(s.bpm) / 4.0;
    const int st = static_cast<int>(static_cast<double>(pos_) / samples_per_step);
    if (st != step_) {
        step_ = st;
        step();
    }
    ++pos_;
    const float dt = 1.f / sr;

    // Pad: three soft saws, filtered dark.
    float pad = 0.f;
    for (int k = 0; k < 3; ++k) {
        pad_phase_[k] += pad_freq_[k] * (1.0 + 0.002 * (k - 1)) / sr;
        pad_phase_[k] -= std::floor(pad_phase_[k]);
        pad += static_cast<float>(pad_phase_[k]) * 2.f - 1.f;
    }
    pad_lp_ += (pad / 3.f - pad_lp_) * lowpass(900.f);

    // Bass: a pulse, plucked.
    bass_phase_ += bass_freq_ / sr;
    bass_phase_ -= std::floor(bass_phase_);
    const float pulse = bass_phase_ < 0.3 ? 1.f : -1.f;
    bass_lp_ += (pulse - bass_lp_) * lowpass(300.f + 1500.f * bass_env_);
    const float bass = bass_lp_ * (0.35f + 0.65f * bass_env_);
    bass_env_ *= std::exp(-dt / 0.18f);

    // Lead: square to saw, with a vibrato that sets in on long notes.
    vibrato_ += 5.5 / sr;
    const float vib = 1.f + 0.006f * std::min(1.f, lead_age_ / 0.4f) * static_cast<float>(std::sin(two_pi * vibrato_));
    lead_phase_ += lead_freq_ * vib / sr;
    lead_phase_ -= std::floor(lead_phase_);
    const float square = lead_phase_ < 0.5 ? 1.f : -1.f, saw = static_cast<float>(lead_phase_) * 2.f - 1.f;
    lead_lp_ += (square + (saw - square) * s.lead_bright - lead_lp_) * lowpass(2500.f);
    const float attack = std::min(1.f, lead_age_ / 0.01f);
    const float lead = lead_lp_ * attack * (0.45f + 0.55f * lead_env_) * std::exp(-lead_age_ / 1.2f);
    lead_env_ *= std::exp(-dt / 0.15f);
    lead_age_ += dt;

    // Drums: a kick sweeping down, a snare of noise and tone, a hat.
    float drums = 0.f;
    if (kick_env_ > 1e-3f) {
        kick_phase_ += (50.0 + 110.0 * std::exp(-kick_age_ / 0.03f)) / sr;
        drums += static_cast<float>(std::sin(two_pi * kick_phase_)) * kick_env_;
        kick_env_ *= std::exp(-dt / 0.12f);
        kick_age_ += dt;
    }
    const float n = noise();
    if (snare_env_ > 1e-3f) {
        snare_lp_ += (n - snare_lp_) * lowpass(3500.f);
        drums += snare_lp_ * snare_env_ * 0.5f;
        snare_env_ *= std::exp(-dt / 0.09f);
    }
    if (hat_env_ > 1e-3f) {
        hat_lp_ += (n - hat_lp_) * lowpass(6000.f);
        drums += (n - hat_lp_) * hat_env_ * 0.25f;
        hat_env_ *= std::exp(-dt / 0.025f);
    }
    return 0.22f * pad_lp_ + 0.35f * bass + 0.22f * lead + 0.45f * drums;
}

} // namespace racer
