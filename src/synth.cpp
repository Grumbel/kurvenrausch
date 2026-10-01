// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "synth.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace racer {

namespace {

constexpr float sr = static_cast<float>(Synth::sample_rate);
constexpr double two_pi = 6.283185307179586;

constexpr float idle_rpm = 1000.f;
constexpr float rpm_range = 6500.f;     // idle + range = rev limit, 7500 rpm
constexpr int cylinders = 6;            // three firings per revolution
constexpr int max_harmonics = 12;

// One-pole smoothing coefficient for a time constant in seconds.
float smoothing(float seconds) { return 1.f - std::exp(-1.f / (sr * seconds)); }
// One-pole low-pass coefficient for a cutoff in Hz.
float lowpass_coeff(float hz) { return 1.f - std::exp(-6.2831853f * hz / sr); }

} // namespace

Synth::Synth() = default;

void Synth::set_params(const SynthParams& p) {
    rpm_.store(p.rpm, std::memory_order_relaxed);
    throttle_.store(p.throttle, std::memory_order_relaxed);
    speed_.store(p.speed, std::memory_order_relaxed);
    skid_.store(p.skid, std::memory_order_relaxed);
    gravel_.store(p.gravel, std::memory_order_relaxed);
    scrape_.store(p.scrape, std::memory_order_relaxed);
    rain_.store(p.rain, std::memory_order_relaxed);
    volume_.store(p.volume, std::memory_order_relaxed);
}

void Synth::trigger_crash(float intensity) {
    crash_intensity_.store(std::clamp(intensity, 0.f, 1.f), std::memory_order_relaxed);
    crash_events_.fetch_add(1, std::memory_order_release);
}

float Synth::noise() {
    // xorshift32, scaled to [-1, 1)
    rng_ ^= rng_ << 13;
    rng_ ^= rng_ >> 17;
    rng_ ^= rng_ << 5;
    return static_cast<float>(rng_) / 2147483648.f - 1.f;
}

void Synth::render(int16_t* out, int frames) {
    const auto load = [](const std::atomic<float>& a) { return std::clamp(a.load(std::memory_order_relaxed), 0.f, 1.f); };
    const float t_rpm = load(rpm_), t_throttle = load(throttle_), t_speed = load(speed_);
    const float t_skid = load(skid_), t_gravel = load(gravel_), t_scrape = load(scrape_);
    const float t_rain = load(rain_), t_volume = load(volume_);

    if (!primed_) { // start from the current state instead of fading in from silence
        s_rpm_ = t_rpm; s_throttle_ = t_throttle; s_speed_ = t_speed; s_skid_ = t_skid;
        s_gravel_ = t_gravel; s_scrape_ = t_scrape; s_rain_ = t_rain; s_volume_ = t_volume;
        primed_ = true;
    }

    const int events = crash_events_.load(std::memory_order_acquire);
    if (events != crash_seen_) {
        crash_seen_ = events;
        crash_env_ = std::max(crash_env_, crash_intensity_.load(std::memory_order_relaxed));
        thump_phase_ = 0.0;
    }

    // Smoothing: revs follow the pedals with some inertia, noise sources fade
    // in and out quickly enough to feel immediate but without clicks.
    const float a_rpm = smoothing(0.07f), a_load = smoothing(0.05f), a_fast = smoothing(0.012f);
    const float a_vol = smoothing(0.02f), a_speed = smoothing(0.1f);
    const float crash_decay = std::exp(-1.f / (sr * 0.22f));
    const float thump_decay = std::exp(-1.f / (sr * 0.12f));
    float thump_env = crash_env_ * crash_env_;

    for (int i = 0; i < frames; ++i, ++samples_) {
        // Harmonic amplitudes depend on the load (open throttle is brighter) and
        // change slowly, so a table every 64 samples is plenty.
        if ((samples_ & 63u) == 0) {
            const float p = 1.5f - 0.75f * s_throttle_;
            for (int k = 1; k <= max_harmonics; ++k) amp_[k] = 1.f / std::pow(static_cast<float>(k), p);
        }

        s_rpm_ += (t_rpm - s_rpm_) * a_rpm;
        s_throttle_ += (t_throttle - s_throttle_) * a_load;
        s_speed_ += (t_speed - s_speed_) * a_speed;
        s_skid_ += (t_skid - s_skid_) * a_fast;
        s_gravel_ += (t_gravel - s_gravel_) * a_fast;
        s_scrape_ += (t_scrape - s_scrape_) * a_fast;
        s_rain_ += (t_rain - s_rain_) * a_vol;
        s_volume_ += (t_volume - s_volume_) * a_vol;

        // ---- Engine ------------------------------------------------------
        const float rpm = idle_rpm + s_rpm_ * rpm_range;
        const float fire_hz = rpm / 60.f * static_cast<float>(cylinders) / 2.f;
        jitter_ += (noise() - jitter_) * 0.002f;            // slow irregularity
        crank_ += static_cast<double>(fire_hz / static_cast<float>(cylinders / 2) / sr) * (1.0 + 0.012 * jitter_);
        crank_ -= std::floor(crank_);

        // Harmonics of the firing frequency (three firings per revolution),
        // plus the lumpy once and twice per revolution components.
        const int kmax = std::max(2, std::min(max_harmonics, static_cast<int>(9000.f / fire_hz)));
        float e = 0.f;
        for (int k = 1; k <= kmax; ++k) {
            e += amp_[k] * static_cast<float>(std::sin(two_pi * 3.0 * k * crank_ + 1.3 * k));
        }
        e += 0.35f * static_cast<float>(std::sin(two_pi * crank_ + 0.4));
        e += 0.18f * static_cast<float>(std::sin(two_pi * 2.0 * crank_ + 1.1));
        e = std::tanh(e * (0.7f + 0.9f * s_throttle_));       // exhaust grit

        intake_ += (noise() - intake_) * lowpass_coeff(500.f + 2500.f * s_rpm_);
        const float intake = intake_ * (0.08f + 0.5f * s_throttle_ * (0.4f + s_rpm_));
        const float engine = (e * (0.5f + 0.5f * s_throttle_) + intake) * 0.42f;

        // ---- Road, wind, gravel, rain ----------------------------------------
        const float n = noise();
        road_ += (n - road_) * lowpass_coeff(250.f + 700.f * s_speed_);
        const float roar = road_ * 0.8f * std::pow(s_speed_, 1.2f);

        // Wind: a band-pass state variable filter whose centre rises with speed.
        {
            const float f = 2.f * std::sin(3.14159265f * (500.f + 1200.f * s_speed_) / sr);
            wind_low_ += f * wind_band_;
            const float high = n - wind_low_ - 1.4f * wind_band_;
            wind_band_ += f * high;
        }
        const float wind = wind_band_ * 0.4f * s_speed_ * s_speed_;

        // Gravel: heavy-tailed noise (cubed) gives the crackle.
        const float crackle = n * n * n;
        gravel_lp_ += (crackle - gravel_lp_) * lowpass_coeff(3000.f);
        const float gravel = gravel_lp_ * s_gravel_ * (0.25f + 0.9f * s_speed_) * 1.4f;

        rain_lp_ += (n - rain_lp_) * lowpass_coeff(2500.f);
        const float rain = (n - rain_lp_) * s_rain_ * (0.09f + 0.04f * s_speed_);

        // ---- Tyre squeal: narrow band-pass noise wandering around 1.2 kHz --
        lfo_ += 7.0 / sample_rate;
        lfo_ -= std::floor(lfo_);
        const float centre = 1150.f + 220.f * static_cast<float>(std::sin(two_pi * lfo_)) + 80.f * jitter_ * 20.f;
        {
            const float f = 2.f * std::sin(3.14159265f * centre / sr);
            skid_low_ += f * skid_band_;
            const float high = n - skid_low_ - (1.f / 14.f) * skid_band_;
            skid_band_ += f * high;
        }
        const float skid = skid_band_ * 0.5f * std::pow(s_skid_, 1.3f);

        // ---- Scraping metal: detuned saws and bright noise, with tremolo ---
        float scrape = 0.f;
        if (s_scrape_ > 0.001f) {
            scrape_wobble_ += (noise() - scrape_wobble_) * 0.0005f;
            const double base[3] = {620.0, 933.0, 1480.0};
            for (int k = 0; k < 3; ++k) {
                scrape_phase_[k] += base[k] * (1.0 + 0.06 * scrape_wobble_ * 25.0) / sample_rate;
                scrape_phase_[k] -= std::floor(scrape_phase_[k]);
                scrape += static_cast<float>(2.0 * scrape_phase_[k] - 1.0) * (0.5f - 0.12f * static_cast<float>(k));
            }
            const float tremolo = 0.7f + 0.3f * static_cast<float>(std::sin(two_pi * lfo_ * 5.0));
            scrape = (scrape * 0.6f + (n - rain_lp_) * 0.35f) * s_scrape_ * tremolo;
        }

        // ---- Crash: a burst of low noise and a thump ---------------------------
        float crash = 0.f;
        if (crash_env_ > 1e-4f) {
            crash_lp_ += (n - crash_lp_) * lowpass_coeff(1400.f);
            thump_phase_ += 62.0 / sample_rate;
            crash = crash_lp_ * crash_env_ * 2.2f +
                    static_cast<float>(std::sin(two_pi * thump_phase_)) * thump_env * 1.4f +
                    static_cast<float>(std::sin(two_pi * 410.0 * thump_phase_ / 62.0)) * thump_env * 0.15f;
            crash_env_ *= crash_decay;
            thump_env *= thump_decay;
        }

        const float mix = engine + roar + wind + gravel + rain + skid + scrape + crash;
        const float x = std::tanh(mix * s_volume_ * 1.1f);
        out[i] = static_cast<int16_t>(std::lround(x * 30000.f));
    }
}

bool write_wav(const std::string& path, const std::vector<int16_t>& samples, int sample_rate) {
    std::FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    const auto u32 = [&](uint32_t v) { for (int i = 0; i < 4; ++i) std::fputc(static_cast<int>((v >> (8 * i)) & 0xff), f); };
    const auto u16 = [&](uint16_t v) { for (int i = 0; i < 2; ++i) std::fputc(static_cast<int>((v >> (8 * i)) & 0xff), f); };
    const uint32_t bytes = static_cast<uint32_t>(samples.size() * 2);
    std::fwrite("RIFF", 1, 4, f);
    u32(36 + bytes);
    std::fwrite("WAVEfmt ", 1, 8, f);
    u32(16);
    u16(1);                                         // PCM
    u16(1);                                         // mono
    u32(static_cast<uint32_t>(sample_rate));
    u32(static_cast<uint32_t>(sample_rate) * 2);    // byte rate
    u16(2);                                         // block align
    u16(16);                                        // bits per sample
    std::fwrite("data", 1, 4, f);
    u32(bytes);
    for (int16_t s : samples) u16(static_cast<uint16_t>(s));
    return std::fclose(f) == 0;
}

} // namespace racer
