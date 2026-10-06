// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <cstring>
#include <string>

namespace racer {
namespace frame_stats {

// Fixed set of timed phases for one frame (ms). Nested work is attributed to
// the innermost phase that is currently open.
enum class Phase : int {
    Sim = 0,       // fixed_update / world
    GlesSetup,     // FBO bind, clear, slice project
    GlesBackdrop,  // sky / mountains / sun
    GlesRoad,      // road strips, tunnels, beyond
    GlesSprites,   // rails, cliffs, scenery, cars
    GlesLight,     // night lightmap: lamp pools, headlight
    GlesCompose,   // night compose (albedo × lightmap)
    GlesMirror,    // the rear-view mirror's scene (its own phases fold into it)
    Hud,           // CPU HUD / cockpit / menus into fb_
    Present,       // upload + composite + swap
    Count
};

inline const char* phase_name(Phase p) {
    switch (p) {
        case Phase::Sim: return "sim";
        case Phase::GlesSetup: return "g-setup";
        case Phase::GlesBackdrop: return "g-sky";
        case Phase::GlesRoad: return "g-road";
        case Phase::GlesSprites: return "g-spr";
        case Phase::GlesLight: return "g-light";
        case Phase::GlesCompose: return "g-comp";
        case Phase::GlesMirror: return "g-mirror";
        case Phase::Hud: return "hud";
        case Phase::Present: return "present";
        default: return "?";
    }
}

struct Snapshot {
    double ms[static_cast<int>(Phase::Count)]{};
    double ms_frame = 0; // wall time for the whole tick
    int draw_calls = 0;
    int solid_verts = 0;
    int tex_verts = 0;
    int slices = 0;
    int sprites = 0;
    int lamps = 0;
    bool gles = false;
    // Pixels covered by the GLES draws of each phase (overdraw counts each
    // time), and the scene's size: fill / screen_px = screens filled.
    double fill[static_cast<int>(Phase::Count)]{};
    int screen_px = 0;
};

// Called at the start of each game tick before work begins.
void begin_frame();
// Close any open phase and snapshot totals (call just before present returns
// or at end of tick).
void end_frame(double frame_ms);

void begin(Phase p);
void end(Phase p);

void add_draw(int vertex_count, bool textured);
// Pixels a draw covers, charged to the innermost open phase.
void add_fill(double pixels);

// GPU-sync profiling: when set, `finish` (glFinish) runs at the start and end
// of every phase, so each phase's time includes the GPU work it queued
// instead of that work landing in whatever blocks later (usually present).
// Slows the frame down; turned on by KURVENRAUSCH_GPU_SYNC=1.
void set_gpu_sync(void (*finish)());
bool gpu_sync();
void set_scene_counts(int slices, int sprites, int lamps, bool gles, int screen_px = 0);

// One console line: draws, vertices, and per phase its time and the screens
// it filled (e.g. "g-spr=1.2ms/3.4x").
std::string format(const Snapshot& s);

const Snapshot& current(); // in-progress (for mid-frame)
const Snapshot& last();    // completed previous frame

// RAII timer for a phase.
struct Scope {
    Phase phase;
    explicit Scope(Phase p) : phase(p) { begin(p); }
    ~Scope() { end(phase); }
    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;
};

} // namespace frame_stats
} // namespace racer
