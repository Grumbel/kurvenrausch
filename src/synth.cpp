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
    horn_.store(p.horn, std::memory_order_relaxed);
    nitro_.store(p.nitro, std::memory_order_relaxed);
    engine_.store(p.engine, std::memory_order_relaxed);
    pump_.store(p.pump, std::memory_order_relaxed);
    splash_.store(p.splash, std::memory_order_relaxed);
}

void Synth::trigger_ding() {
    ding_events_.fetch_add(1, std::memory_order_release);
}

void Synth::trigger_crash(float intensity) {
    crash_intensity_.store(std::clamp(intensity, 0.f, 1.f), std::memory_order_relaxed);
    crash_events_.fetch_add(1, std::memory_order_release);
}

void Synth::trigger_thunder(float intensity) {
    thunder_intensity_.store(std::clamp(intensity, 0.f, 1.f), std::memory_order_relaxed);
    thunder_events_.fetch_add(1, std::memory_order_release);
}

void Synth::trigger_whoosh(float intensity) {
    whoosh_intensity_.store(std::clamp(intensity, 0.f, 1.f), std::memory_order_relaxed);
    whoosh_events_.fetch_add(1, std::memory_order_release);
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
    const float t_horn = load(horn_), t_nitro = load(nitro_), t_engine = load(engine_), t_pump = load(pump_);
    const float t_splash = load(splash_);

    if (!primed_) { // start from the current state instead of fading in from silence
        s_rpm_ = t_rpm; s_throttle_ = t_throttle; s_speed_ = t_speed; s_skid_ = t_skid;
        s_gravel_ = t_gravel; s_scrape_ = t_scrape; s_rain_ = t_rain; s_volume_ = t_volume;
        s_horn_ = t_horn; s_nitro_ = t_nitro; s_engine_ = t_engine; s_pump_ = t_pump; s_splash_ = t_splash;
        slow_throttle_ = t_throttle;
        primed_ = true;
    }

    const int events = crash_events_.load(std::memory_order_acquire);
    if (events != crash_seen_) {
        crash_seen_ = events;
        crash_env_ = std::max(crash_env_, crash_intensity_.load(std::memory_order_relaxed));
        thump_phase_ = 0.0;
    }
    const int dings = ding_events_.load(std::memory_order_acquire);
    if (dings != ding_seen_) {
        ding_seen_ = dings;
        ding_age_ = 0.f;
        ding_phase_[0] = ding_phase_[1] = 0.0;
    }
    const int thunders = thunder_events_.load(std::memory_order_acquire);
    if (thunders != thunder_seen_) {
        thunder_seen_ = thunders;
        thunder_strength_ = thunder_intensity_.load(std::memory_order_relaxed);
        thunder_age_ = 0.f;
    }
    const int whooshes = whoosh_events_.load(std::memory_order_acquire);
    if (whooshes != whoosh_seen_) {
        whoosh_seen_ = whooshes;
        whoosh_env_ = std::max(whoosh_env_, whoosh_intensity_.load(std::memory_order_relaxed));
        whoosh_age_ = 0.f;
    }

    // Smoothing: revs follow the pedals with some inertia, noise sources fade
    // in and out quickly enough to feel immediate but without clicks.
    const float a_rpm = smoothing(0.07f), a_load = smoothing(0.05f), a_fast = smoothing(0.012f);
    const float a_vol = smoothing(0.02f), a_speed = smoothing(0.1f);
    const float crash_decay = std::exp(-1.f / (sr * 0.22f));
    const float thump_decay = std::exp(-1.f / (sr * 0.12f));
    const float whoosh_decay = std::exp(-1.f / (sr * 0.16f));
    const float a_horn = smoothing(0.006f), a_nitro = smoothing(0.06f);
    const float a_slow = smoothing(0.35f);
    const float overrun_decay = std::exp(-1.f / (sr * 0.45f));
    const float pop_decay = std::exp(-1.f / (sr * 0.004f));
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
        s_horn_ += (t_horn - s_horn_) * a_horn;
        s_nitro_ += (t_nitro - s_nitro_) * a_nitro;
        s_engine_ += (t_engine - s_engine_) * a_load;
        s_pump_ += (t_pump - s_pump_) * a_vol;
        s_splash_ += (t_splash - s_splash_) * a_fast;

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

        // Load transients: opening the throttle surges, lifting off at revs
        // makes the exhaust crackle (also on every upshift).
        slow_throttle_ += (s_throttle_ - slow_throttle_) * a_slow;
        const float surge = std::max(0.f, s_throttle_ - slow_throttle_);
        overrun_ = std::max(overrun_ * overrun_decay, std::max(0.f, slow_throttle_ - s_throttle_) * s_rpm_ * 1.6f);

        // Exhaust pulses: every firing kicks a pipe resonance that follows
        // the revs (the growl) and a low body resonance (the bass). The three
        // firings of a revolution differ a little, which makes the engine
        // lumpy and keeps bass at the revolution rate even at high revs.
        float kick = 0.f;
        const int firing = static_cast<int>(crank_ * 3.0) % 3;
        if (firing != firing_) {
            firing_ = firing;
            constexpr float cylinder[3] = {1.f, 0.7f, 0.86f};
            kick = cylinder[firing] * (0.45f + 0.55f * s_throttle_);
            if (overrun_ > 0.05f && 0.5f + 0.5f * noise() < overrun_) pop_ = std::max(pop_, 0.5f + 0.5f * std::abs(noise()));
        }
        const auto ring = [](float* y, float x, float hz, float r) {
            const float c = 2.f * r * std::cos(6.2831853f * hz / sr);
            const float out = c * y[0] - r * r * y[1] + x;
            y[1] = y[0];
            y[0] = out;
            return out;
        };
        const float pipe = ring(pipe_, kick, std::min(2.f * fire_hz, 900.f), 0.992f) * 0.035f;
        const float body = ring(body_, kick, 68.f, 0.996f) * 0.05f;

        // Overrun pops: short bright bursts with a thud.
        float pops = 0.f;
        if (pop_ > 1e-3f) {
            const float burst = noise() * pop_;
            pop_lp_ += (burst - pop_lp_) * lowpass_coeff(2500.f);
            pops = (burst - pop_lp_) * 0.9f + pop_lp_ * 0.6f;
            pop_ *= pop_decay;
        }

        intake_ += (noise() - intake_) * lowpass_coeff(500.f + 2500.f * s_rpm_);
        const float intake = intake_ * (0.08f + 0.5f * s_throttle_ * (0.4f + s_rpm_));
        // Louder as it revs, and with the load; a surge when the throttle opens.
        const float level = (0.5f + 0.55f * s_rpm_) * (1.f + 2.2f * surge);
        const float engine = ((e * (0.4f + 0.4f * s_throttle_) + pipe + body * (0.6f + 0.4f * s_throttle_) + intake) * level +
                              pops) * 0.27f * s_engine_;

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

        // ---- Thunder: a crack, then a low rumble rolling on for seconds -----
        float thunder = 0.f;
        if (thunder_age_ < 6.f) {
            const float age = thunder_age_;
            thunder_crack_lp_ += (n - thunder_crack_lp_) * lowpass_coeff(2500.f);
            const float crack = (n - thunder_crack_lp_) * thunder_strength_ * thunder_strength_ * std::exp(-age / 0.07f);
            thunder_low_[0] += (n - thunder_low_[0]) * lowpass_coeff(160.f);
            thunder_low_[1] += (thunder_low_[0] - thunder_low_[1]) * lowpass_coeff(90.f);
            // The roll: the rumble swells and fades in irregular waves.
            if ((samples_ & 1023u) == 0) thunder_roll_ = 0.55f + 0.45f * std::abs(noise());
            const float env = std::min(1.f, age / 0.15f) * std::exp(-age / 1.6f) * (0.4f + 0.6f * thunder_strength_);
            thunder = crack * 0.8f + thunder_low_[1] * env * thunder_roll_ * 9.f;
            thunder_age_ += 1.f / sr;
        }

        // ---- Horn: two tones a major third apart, square-ish and filtered ---
        float horn = 0.f;
        if (s_horn_ > 1e-4f) {
            const double tones[2] = {415.0, 523.0};
            float h = 0.f;
            for (int k = 0; k < 2; ++k) {
                horn_phase_[k] += tones[k] / sample_rate;
                horn_phase_[k] -= std::floor(horn_phase_[k]);
                h += std::tanh(3.f * static_cast<float>(std::sin(two_pi * horn_phase_[k])));
            }
            horn_lp_ += (h - horn_lp_) * lowpass_coeff(2200.f);
            horn = horn_lp_ * 0.22f * s_horn_;
        }

        // ---- Nitro: a deep burner roar with a hiss on top --------------------
        float nitro = 0.f;
        if (s_nitro_ > 1e-4f) {
            nitro_lp_ += (n - nitro_lp_) * lowpass_coeff(700.f);
            nitro_rumble_ += (nitro_lp_ - nitro_rumble_) * lowpass_coeff(120.f);
            nitro = (nitro_lp_ * 1.1f + nitro_rumble_ * 2.f + (n - rain_lp_) * 0.12f) * s_nitro_;
        }

        // ---- Whoosh: band-passed noise sweeping down as the car goes by ----
        float whoosh = 0.f;
        if (whoosh_env_ > 1e-4f) {
            whoosh_age_ += 1.f / sr;
            const float centre_hz = 400.f + 1800.f * std::exp(-whoosh_age_ * 9.f);
            const float f = 2.f * std::sin(3.14159265f * centre_hz / sr);
            whoosh_low_ += f * whoosh_band_;
            const float high = n - whoosh_low_ - 0.7f * whoosh_band_;
            whoosh_band_ += f * high;
            const float attack = std::min(1.f, whoosh_age_ / 0.04f);
            whoosh = whoosh_band_ * 3.5f * whoosh_env_ * attack;
            whoosh_env_ *= whoosh_decay;
        }

        // ---- Fuel pump: a motor hum with the gurgle of the fuel ----------
        float pump = 0.f;
        if (s_pump_ > 1e-4f) {
            pump_phase_ += 100.0 / sample_rate;
            pump_phase_ -= std::floor(pump_phase_);
            pump_lp_ += (n - pump_lp_) * lowpass_coeff(350.f);
            const float hum = static_cast<float>(std::sin(two_pi * pump_phase_) + 0.4 * std::sin(two_pi * 2.0 * pump_phase_));
            const float gurgle = pump_lp_ * (0.6f + 0.4f * static_cast<float>(std::sin(two_pi * lfo_ * 3.0)));
            pump = (hum * 0.2f + gurgle * 0.8f) * s_pump_;
        }

        // ---- Splash: a broad hiss of spray with a fluttering wash ---------
        float splash = 0.f;
        if (s_splash_ > 1e-4f) {
            const float f = 2.f * std::sin(3.14159265f * 1400.f / sr);
            splash_low_ += f * splash_band_;
            const float high = n - splash_low_ - 1.2f * splash_band_;
            splash_band_ += f * high;
            splash_flutter_ += (std::abs(noise()) - splash_flutter_) * 0.004f;
            splash = splash_band_ * (0.6f + 1.4f * splash_flutter_) * 0.9f * s_splash_;
        }

        // ---- Chime: two bell notes, the second a little later ------------
        float ding = 0.f;
        if (ding_age_ < 2.f) {
            ding_age_ += 1.f / sr;
            const double notes[2] = {1318.5, 1046.5}; // E6, C6
            for (int k = 0; k < 2; ++k) {
                const float age = ding_age_ - 0.18f * static_cast<float>(k);
                if (age < 0.f) continue;
                ding_phase_[k] += notes[k] / sample_rate;
                ding += static_cast<float>(std::sin(two_pi * ding_phase_[k])) * 0.25f * std::exp(-age * 5.f);
            }
        }

        const float mix = engine + roar + wind + gravel + rain + skid + scrape + crash + horn + nitro + whoosh + pump + ding +
                          splash + thunder;
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
