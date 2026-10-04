// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "animals.hpp"

namespace racer {

const AnimalInfo& animal_info(Animal kind) {
    static const AnimalInfo infos[animal_kinds] = {
        //              width   speed  herd
        /* Cow      */ {800.f, 0.25f, 1},
        /* Giraffe  */ {900.f, 0.30f, 1},
        /* Deer     */ {700.f, 0.60f, 2},
        /* Sheep    */ {500.f, 0.30f, 3},
        /* Kangaroo */ {450.f, 0.90f, 1},
    };
    return infos[static_cast<int>(kind) % animal_kinds];
}

Animal zone_animal(Decor decor) {
    switch (decor) {
        case Decor::Rajasthan: return Animal::Cow;
        case Decor::Savanna: return Animal::Giraffe;
        case Decor::Forest:
        case Decor::Alpine:
        case Decor::Autumn: return Animal::Deer;
        case Decor::Country: return Animal::Sheep;
        case Decor::Outback: return Animal::Kangaroo;
        default: return Animal::None; // towns, deserts, coasts, beaches
    }
}

} // namespace racer
