// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "music.hpp"

#include <atomic>
#include <cstdint>
#include <string>
#include <vector>

namespace racer {

// What the car and the world sound like right now. All values 0 .. 1.
struct SynthParams {
    float rpm = 0.f;       // normalised engine revs (drivetrain::rpm); 0 is idle
    float throttle = 0.f;  // engine load
    float speed = 0.f;     // fraction of top speed: road roar and wind
    float skid = 0.f;      // tyre squeal
    float gravel = 0.f;    // driving on the verge
    float scrape = 0.f;    // metal on a barrier
    float rain = 0.f;      // rain hiss
    float horn = 0.f;      // the player's horn, on or off
    float nitro = 0.f;     // the roar of a nitro burn
    float engine = 1.f;    // 1 running, 0 off (out of fuel); in between it sputters
    float pump = 0.f;      // a fuel pump running beside the car
    float splash = 0.f;    // tyres ploughing through water
    float siren = 0.f;     // a police siren, louder the nearer
    float volume = 1.f;       // master volume, 0 mutes (the mute switch)
    float engine_volume = 1.f; // engine and world SFX, 0 .. 1
    float music_volume = 1.f;  // radio level, 0 .. 1
};


// Real-time sound synthesis, entirely on the fly: there are no samples. The
// engine is an additive six-cylinder whose pitch follows the revs and whose
// brightness follows the load; tyres, wind, gravel, rain, scraping and the
// crash are shaped noise; the horn is a two-tone chord. No SDL in here, so it
// can be rendered offline and analysed. Parameters are atomics: set_params()
// and the triggers may be called from the game thread while render() runs on
// the audio thread.
class Synth {
public:
    static constexpr int sample_rate = 44100;

    Synth();

    void set_params(const SynthParams& params);
    void trigger_crash(float intensity);
    // A car rushing past close by: a short noise sweep falling in pitch.
    void trigger_whoosh(float intensity);
    // A two-note chime, as when the tank is full.
    void trigger_ding();
    // The indicator relay: a tick as the lamps go on, a softer one off.
    void trigger_tick(bool on);
    // Thunder: a crack (the louder the nearer, `intensity` 0 .. 1) and a long,
    // rolling rumble.
    void trigger_thunder(float intensity);
    // The radio: a Music track, -1 for off. A new track starts from its
    // beginning.
    void set_music(int track) { music_track_.store(track, std::memory_order_relaxed); }

    // Renders mono 16-bit samples. For constant parameters the output depends
    // only on the number of samples rendered so far, not on how the calls are
    // chunked. Parameter changes take effect at the next call.
    void render(int16_t* out, int frames);

private:
    float noise();

    // Parameters (written by the game thread, read by render()).
    std::atomic<float> rpm_{0.f}, throttle_{0.f}, speed_{0.f}, skid_{0.f};
    std::atomic<float> gravel_{0.f}, scrape_{0.f}, rain_{0.f}, volume_{1.f};
    std::atomic<float> engine_volume_{1.f}, music_volume_{1.f};
    std::atomic<float> horn_{0.f}, nitro_{0.f}, engine_{1.f}, pump_{0.f}, splash_{0.f}, siren_{0.f};
    std::atomic<float> crash_intensity_{0.f}, whoosh_intensity_{0.f}, thunder_intensity_{0.f};
    std::atomic<int> crash_events_{0}, whoosh_events_{0}, ding_events_{0}, thunder_events_{0};
    std::atomic<int> music_track_{-1};
    std::atomic<int> tick_events_{0};
    std::atomic<bool> tick_on_{true};

    // Smoothed parameters and DSP state (audio thread only).
    bool primed_ = false;
    float s_rpm_ = 0.f, s_throttle_ = 0.f, s_speed_ = 0.f, s_skid_ = 0.f;
    float s_gravel_ = 0.f, s_scrape_ = 0.f, s_rain_ = 0.f, s_volume_ = 1.f;
    float s_engine_volume_ = 1.f, s_music_volume_ = 1.f;
    float s_horn_ = 0.f, s_nitro_ = 0.f, s_engine_ = 1.f, s_pump_ = 0.f, s_splash_ = 0.f, s_siren_ = 0.f;
    // The radio playing (0 .. 1, eased), and the engine's low band: while
    // it plays the engine makes room for it in the middle (see render()).
    float s_radio_ = 0.f;
    float engine_low_ = 0.f;
    double siren_phase_ = 0.0, siren_sweep_ = 0.0;
    float siren_lp_ = 0.f;
    uint32_t rng_ = 0x2545f491u;
    double crank_ = 0.0;          // crank phase, 0 .. 1 per revolution
    float jitter_ = 0.f;
    int firing_ = 0;              // which of the three firings per revolution came last
    float pipe_[2] = {0.f, 0.f};  // exhaust pipe resonance, rings with every firing
    float body_[2] = {0.f, 0.f};  // low body resonance: the bass
    float slow_throttle_ = 0.f;   // a slow follower of the throttle, for surge and overrun
    float overrun_ = 0.f;         // crackle on the overrun after lifting off at revs
    float pop_ = 0.f, pop_lp_ = 0.f;
    float splash_low_ = 0.f, splash_band_ = 0.f, splash_flutter_ = 0.f;
    float intake_ = 0.f, road_ = 0.f, gravel_lp_ = 0.f, rain_lp_ = 0.f, crash_lp_ = 0.f;
    float wind_low_ = 0.f, wind_band_ = 0.f;
    float skid_low_ = 0.f, skid_band_ = 0.f;
    double lfo_ = 0.0, scrape_phase_[3] = {0.0, 0.0, 0.0};
    float scrape_wobble_ = 0.f;
    uint64_t samples_ = 0;        // rendered so far, paces the slow updates
    float amp_[13] = {};          // engine harmonic amplitudes
    int crash_seen_ = 0;
    float crash_env_ = 0.f;
    double thump_phase_ = 0.0;
    double horn_phase_[2] = {0.0, 0.0};
    float horn_lp_ = 0.f;
    float nitro_lp_ = 0.f, nitro_rumble_ = 0.f;
    int whoosh_seen_ = 0;
    float whoosh_env_ = 0.f, whoosh_age_ = 0.f;
    float whoosh_low_ = 0.f, whoosh_band_ = 0.f;
    double pump_phase_ = 0.0;
    float pump_lp_ = 0.f;
    int tick_seen_ = 0;
    float tick_age_ = 1.f, tick_level_ = 0.f;
    double tick_phase_ = 0.0;
    int ding_seen_ = 0;
    float ding_age_ = 10.f;
    double ding_phase_[2] = {0.0, 0.0};
    Music music_;
    int thunder_seen_ = 0;
    float thunder_strength_ = 0.f, thunder_age_ = 100.f;
    float thunder_low_[2] = {0.f, 0.f}, thunder_crack_lp_ = 0.f, thunder_roll_ = 0.f;
};

// Writes mono 16-bit PCM as a WAV file.
bool write_wav(const std::string& path, const std::vector<int16_t>& samples, int sample_rate);

} // namespace racer
