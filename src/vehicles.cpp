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

const CarModel& car_model(int index) {
    static const CarModel models[car_models] = {
        // name         top    accel  grip
        {"SPIDER",      1.00f, 1.00f, 1.00f}, // the red convertible: balanced
        {"GT COUPE",    1.12f, 0.85f, 0.92f}, // fastest flat out, slow to get there
        {"HOT HATCH",   0.88f, 1.30f, 1.12f}, // quick away and sure-footed
        {"MUSCLE",      1.06f, 1.22f, 0.78f}, // brute force, little grip
        {"BIG RIG",     0.84f, 0.62f, 1.25f}, // the truck: slow, but planted
    };
    return models[((index % car_models) + car_models) % car_models];
}

float rival_speed(float cruise, float max_speed, float gap) {
    const bool racing = gap > -rival_race_range / 4.f && gap < rival_race_range;
    return racing ? std::max(cruise, rival_race_speed * max_speed) : cruise;
}

} // namespace racer
