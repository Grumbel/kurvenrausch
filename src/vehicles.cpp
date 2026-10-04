// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "vehicles.hpp"

#include <algorithm>

namespace racer {

const VehicleInfo& vehicle_info(Vehicle kind) {
    static const VehicleInfo infos[] = {
        //              width   speeds        styles share
        /* Car   */ {600.f, 0.25f, 0.60f, 7, 0.35f},
        /* Van   */ {650.f, 0.25f, 0.45f, 3, 0.14f},
        /* Truck */ {700.f, 0.20f, 0.35f, 3, 0.13f},
        /* Rival */ {620.f, 0.78f, 0.90f, 2, 0.05f},
        /* Police*/ {600.f, 0.f,   0.f,   1, 0.f},
        /* Hatch */ {540.f, 0.25f, 0.55f, 5, 0.16f},
        /* Pickup*/ {640.f, 0.28f, 0.55f, 3, 0.10f},
        /* Bus   */ {720.f, 0.18f, 0.30f, 3, 0.07f},
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
    // The order is the saved choices' numbering: new models go at the end.
    using B = Body;
    using R = CarRange;
    static const CarModel models[car_models] = {
        // name         top    accel  grip   body        range       width
        {"SPIDER",      1.00f, 1.00f, 1.00f, B::Car,      R::Sports,  600.f}, // the red convertible: balanced
        {"GT COUPE",    1.12f, 0.85f, 0.92f, B::Car,      R::Sports,  600.f}, // fastest flat out, slow to get there
        {"HOT HATCH",   0.88f, 1.30f, 1.12f, B::Car,      R::Regular, 600.f}, // quick away and sure-footed
        {"MUSCLE",      1.06f, 1.22f, 0.78f, B::Car,      R::Sports,  600.f}, // brute force, little grip
        {"BIG RIG",     0.84f, 0.62f, 1.25f, B::Rig,      R::Trucks,  600.f}, // slow, but planted
        {"SALOON",      0.90f, 0.95f, 1.04f, B::Car,      R::Regular, 600.f}, // the everyday car
        {"TAXI",        0.92f, 1.04f, 0.98f, B::Car,      R::Regular, 600.f}, // a saloon, tuned a little
        {"ESTATE",      0.86f, 0.90f, 1.14f, B::Car,      R::Regular, 600.f}, // the family car: steady
        {"PATROL",      1.02f, 1.05f, 0.95f, B::Police,   R::Regular, 600.f}, // a police car, retired
        {"RACER",       1.18f, 1.12f, 0.84f, B::Racer,    R::Sports,  620.f}, // the rivals' car: fastest, nervous
        {"VAN",         0.86f, 0.80f, 1.15f, B::Van,      R::Trucks,  650.f}, // roomy, slow off the line
        {"BOX TRUCK",   0.78f, 0.55f, 1.30f, B::BoxTruck, R::Trucks,  700.f}, // slow, and steady
        {"MINI",        0.80f, 1.25f, 1.20f, B::Hatch,    R::Regular, 540.f}, // small, nippy, sticks to the road
        {"PICKUP",      0.92f, 0.98f, 1.02f, B::Pickup,   R::Trucks,  640.f}, // a workhorse
        {"COACH",       0.76f, 0.50f, 1.35f, B::Bus,      R::Trucks,  720.f}, // a bus: the slowest and steadiest of all
        {"ROADSTER",    1.04f, 1.10f, 0.90f, B::Car,      R::Sports,  600.f}, // the golden convertible
        {"SUPERCAR",    1.19f, 1.00f, 0.88f, B::Racer,    R::Sports,  620.f}, // the fastest flat out
    };
    return models[((index % car_models) + car_models) % car_models];
}

bool body_shows_people(Body body) { return body == Body::Car || body == Body::Rig || body == Body::Police; }

bool body_has_lightbar(Body body) { return body == Body::Police; }

bool body_is_tall(Body body) {
    return body == Body::Rig || body == Body::Van || body == Body::BoxTruck || body == Body::Bus;
}

int next_car_in_range(int current, CarRange range, int step) {
    const int dir = step < 0 ? -1 : 1;
    const bool inside = car_model(current).range == range;
    int i = inside ? current : (dir > 0 ? car_models - 1 : 0);
    for (int n = 0; n < car_models; ++n) {
        i = ((i + dir) % car_models + car_models) % car_models;
        if (car_model(i).range == range) return i;
    }
    return current;
}

float rival_speed(float cruise, float max_speed, float gap) {
    const bool racing = gap > -rival_race_range / 4.f && gap < rival_race_range;
    return racing ? std::max(cruise, rival_race_speed * max_speed) : cruise;
}

} // namespace racer
