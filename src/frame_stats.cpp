// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "frame_stats.hpp"

#include <SDL2/SDL.h>

#include <cstring>
#include <sstream>

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
void (*sync_finish)() = nullptr;

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
    if (phase_depth[i]++ == 0) {
        if (sync_finish) sync_finish(); // earlier GPU work is not this phase's
        phase_start[i] = SDL_GetPerformanceCounter();
    }
}

void end(Phase p) {
    const int i = static_cast<int>(p);
    if (i < 0 || i >= static_cast<int>(Phase::Count)) return;
    if (phase_depth[i] <= 0) return;
    if (--phase_depth[i] == 0) {
        if (sync_finish) sync_finish();
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

void add_fill(double pixels) {
    if (open_n > 0) cur.fill[open_stack[open_n - 1]] += pixels;
}

void set_scene_counts(int slices, int sprites, int lamps, bool gles, int screen_px) {
    cur.screen_px = screen_px;
    cur.slices = slices;
    cur.sprites = sprites;
    cur.lamps = lamps;
    cur.gles = gles;
}

void set_gpu_sync(void (*finish)()) { sync_finish = finish; }
bool gpu_sync() { return sync_finish != nullptr; }

std::string format(const Snapshot& s) {
    std::ostringstream out;
    out << "frame=" << s.ms_frame << "ms draws=" << s.draw_calls << " solidV=" << s.solid_verts
        << " texV=" << s.tex_verts;
    for (int i = 0; i < static_cast<int>(Phase::Count); ++i) {
        const double screens = s.screen_px > 0 ? s.fill[i] / s.screen_px : 0.0;
        if (s.ms[i] < 0.15 && screens < 0.05) continue;
        out << ' ' << phase_name(static_cast<Phase>(i)) << '=' << s.ms[i] << "ms";
        if (screens >= 0.05) out << '/' << static_cast<int>(screens * 10.0 + 0.5) / 10.0 << 'x';
    }
    if (gpu_sync()) out << " (gpu-sync)";
    return out.str();
}

const Snapshot& current() { return cur; }
const Snapshot& last() { return prev; }

} // namespace frame_stats
} // namespace racer
