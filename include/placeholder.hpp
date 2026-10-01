#pragma once
#include "framebuffer.hpp"
#include "track.hpp"

// Temporary flat HUD. To be replaced by a bitmap font HUD.
namespace racer::placeholder {

void draw_hud(Framebuffer& fb, float speed_fraction, int lap);

} // namespace racer::placeholder
