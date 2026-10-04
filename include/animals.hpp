// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "track.hpp"

#include <cstdint>

namespace racer {

// Animals that cross the road now and then, each in its own countryside.
enum class Animal : uint8_t { Cow, Giraffe, Deer, Sheep, Kangaroo, None };
constexpr int animal_kinds = 5;

struct AnimalInfo {
    float width;  // world units
    float speed;  // across the road, road half-widths per second
    int herd;     // how many cross together
};
const AnimalInfo& animal_info(Animal kind);

// The animal that crosses the road in a zone of this decor, or None.
Animal zone_animal(Decor decor);

// Seconds until the next crossing, for `roll` uniform in [0, 1).
constexpr float animal_min_wait = 15.f, animal_max_wait = 40.f;
inline float animal_wait(float roll) { return animal_min_wait + (animal_max_wait - animal_min_wait) * roll; }

} // namespace racer
