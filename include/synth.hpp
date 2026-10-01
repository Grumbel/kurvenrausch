// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

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
    float volume = 1.f;    // master volume, 0 mutes
};

// Real-time sound synthesis, entirely on the fly: there are no samples. The
// engine is an additive six-cylinder whose pitch follows the revs and whose
// brightness follows the load; tyres, wind, gravel, rain, scraping and the
// crash are shaped noise. No SDL in here, so it can be rendered offline and
// analysed. Parameters are atomics: set_params() and trigger_crash() may be
// called from the game thread while render() runs on the audio thread.
class Synth {
public:
    static constexpr int sample_rate = 44100;

    Synth();

    void set_params(const SynthParams& params);
    void trigger_crash(float intensity);

    // Renders mono 16-bit samples. For constant parameters the output depends
    // only on the number of samples rendered so far, not on how the calls are
    // chunked. Parameter changes take effect at the next call.
    void render(int16_t* out, int frames);

private:
    float noise();

    // Parameters (written by the game thread, read by render()).
    std::atomic<float> rpm_{0.f}, throttle_{0.f}, speed_{0.f}, skid_{0.f};
    std::atomic<float> gravel_{0.f}, scrape_{0.f}, rain_{0.f}, volume_{1.f};
    std::atomic<float> crash_intensity_{0.f};
    std::atomic<int> crash_events_{0};

    // Smoothed parameters and DSP state (audio thread only).
    bool primed_ = false;
    float s_rpm_ = 0.f, s_throttle_ = 0.f, s_speed_ = 0.f, s_skid_ = 0.f;
    float s_gravel_ = 0.f, s_scrape_ = 0.f, s_rain_ = 0.f, s_volume_ = 1.f;
    uint32_t rng_ = 0x2545f491u;
    double crank_ = 0.0;          // crank phase, 0 .. 1 per revolution
    float jitter_ = 0.f;
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
};

// Writes mono 16-bit PCM as a WAV file.
bool write_wav(const std::string& path, const std::vector<int16_t>& samples, int sample_rate);

} // namespace racer
