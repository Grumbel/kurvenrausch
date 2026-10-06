// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "frame_stats.hpp"

#include <SDL2/SDL.h>

#include <cstring>

#include <algorithm>

namespace racer {
namespace frame_stats {
namespace {

Snapshot cur{};
Snapshot prev{};
Uint64 phase_start[static_cast<int>(Phase::Count)]{};
int phase_depth[static_cast<int>(Phase::Count)]{};
int open_stack[8]{};
int open_n = 0;

double ticks_to_ms(Uint64 delta) {
    const double freq = static_cast<double>(SDL_GetPerformanceFrequency());
    if (freq <= 0.0) return 0.0;
    return 1000.0 * static_cast<double>(delta) / freq;
}

} // namespace

void begin_frame() {
    cur = Snapshot{};
    open_n = 0;
    std::memset(phase_depth, 0, sizeof phase_depth);
    std::memset(phase_start, 0, sizeof phase_start);
}

void end_frame(double frame_ms) {
    while (open_n > 0) {
        end(static_cast<Phase>(open_stack[open_n - 1]));
    }
    cur.ms_frame = frame_ms;
    prev = cur;
}

void begin(Phase p) {
    const int i = static_cast<int>(p);
    if (i < 0 || i >= static_cast<int>(Phase::Count)) return;
    if (open_n < static_cast<int>(sizeof open_stack / sizeof open_stack[0]))
        open_stack[open_n++] = i;
    if (phase_depth[i]++ == 0) phase_start[i] = SDL_GetPerformanceCounter();
}

void end(Phase p) {
    const int i = static_cast<int>(p);
    if (i < 0 || i >= static_cast<int>(Phase::Count)) return;
    if (phase_depth[i] <= 0) return;
    if (--phase_depth[i] == 0) {
        const Uint64 now = SDL_GetPerformanceCounter();
        cur.ms[i] += ticks_to_ms(now - phase_start[i]);
        phase_start[i] = 0;
    }
    if (open_n > 0 && open_stack[open_n - 1] == i) --open_n;
}

void add_draw(int vertex_count, bool textured) {
    ++cur.draw_calls;
    if (textured) cur.tex_verts += vertex_count;
    else cur.solid_verts += vertex_count;
}

void set_scene_counts(int slices, int sprites, int lamps, bool gles) {
    cur.slices = slices;
    cur.sprites = sprites;
    cur.lamps = lamps;
    cur.gles = gles;
}

const Snapshot& current() { return cur; }
const Snapshot& last() { return prev; }

} // namespace frame_stats
} // namespace racer
