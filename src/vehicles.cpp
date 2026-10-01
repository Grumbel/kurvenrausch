// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "vehicles.hpp"

#include <algorithm>

namespace racer {

const VehicleInfo& vehicle_info(Vehicle kind) {
    static const VehicleInfo infos[] = {
        //              width   speeds        styles share
        /* Car   */ {600.f, 0.25f, 0.60f, 4, 0.55f},
        /* Van   */ {650.f, 0.25f, 0.45f, 3, 0.22f},
        /* Truck */ {700.f, 0.20f, 0.35f, 3, 0.17f},
        /* Rival */ {620.f, 0.78f, 0.90f, 2, 0.06f},
    };
    static_assert(sizeof(infos) / sizeof(infos[0]) == static_cast<size_t>(Vehicle::Count),
                  "vehicle_info() needs an entry for every Vehicle");
    return infos[static_cast<int>(kind)];
}

Vehicle traffic_vehicle(float roll) {
    float sum = 0.f;
    for (int i = 0; i < static_cast<int>(Vehicle::Count); ++i) {
        const auto kind = static_cast<Vehicle>(i);
        sum += vehicle_info(kind).share;
        if (roll < sum) return kind;
    }
    return Vehicle::Car; // rounding at the very top
}

float rival_speed(float cruise, float max_speed, float gap) {
    const bool racing = gap > -rival_race_range / 4.f && gap < rival_race_range;
    return racing ? std::max(cruise, rival_race_speed * max_speed) : cruise;
}

} // namespace racer
