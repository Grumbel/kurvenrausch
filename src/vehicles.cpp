// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#include "vehicles.hpp"

#include <algorithm>
#include <vector>

namespace racer {

const VehicleInfo& vehicle_info(Vehicle kind) {
    static const VehicleInfo infos[] = {
        //              width   speeds        styles share
        /* Car   */ {600.f, 0.25f, 0.60f, 7, 0.33f},
        /* Van   */ {650.f, 0.25f, 0.45f, 3, 0.14f},
        /* Truck */ {700.f, 0.20f, 0.35f, 3, 0.13f},
        /* Rival */ {620.f, 0.78f, 0.90f, 2, 0.05f},
        /* Police*/ {600.f, 0.f,   0.f,   1, 0.f},
        /* Hatch */ {540.f, 0.25f, 0.55f, 5, 0.16f},
        /* Pickup*/ {640.f, 0.28f, 0.55f, 3, 0.10f},
        /* Bus   */ {720.f, 0.18f, 0.30f, 3, 0.07f},
        /* Ambulance*/{650.f, 0.35f, 0.70f, 1, 0.02f},
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
        {"SPIDER",      1.00f, 1.00f, 1.00f, B::Car,      R::Sports,  600.f, region::europe}, // the red convertible: balanced
        {"GT COUPE",    1.12f, 0.85f, 0.92f, B::Car,      R::Sports,  600.f, region::europe}, // fastest flat out, slow to get there
        {"HOT HATCH",   0.88f, 1.30f, 1.12f, B::Car,      R::Regular, 600.f, region::europe}, // quick away and sure-footed
        {"MUSCLE",      1.06f, 1.22f, 0.78f, B::Car,      R::Sports,  600.f, static_cast<uint8_t>(region::america | region::oceania)}, // brute force, little grip
        {"BIG RIG",     0.84f, 0.62f, 1.25f, B::Rig,      R::Trucks,  600.f, static_cast<uint8_t>(region::america | region::oceania)}, // slow, but planted
        {"SALOON",      0.90f, 0.95f, 1.04f, B::Car,      R::Regular, 600.f, static_cast<uint8_t>(region::europe | region::asia)}, // the everyday car
        {"TAXI",        0.92f, 1.04f, 0.98f, B::Car,      R::Regular, 600.f, static_cast<uint8_t>(region::america | region::asia)}, // a saloon, tuned a little
        {"ESTATE",      0.86f, 0.90f, 1.14f, B::Car,      R::Regular, 600.f, static_cast<uint8_t>(region::europe | region::oceania)}, // the family car: steady
        {"PATROL",      1.02f, 1.05f, 0.95f, B::Police,   R::Regular, 600.f, region::america}, // a police car, retired
        {"RACER",       1.18f, 1.12f, 0.84f, B::Racer,    R::Sports,  620.f, region::asia}, // the rivals' car: fastest, nervous
        {"VAN",         0.86f, 0.80f, 1.15f, B::Van,      R::Trucks,  650.f}, // roomy, slow off the line
        {"BOX TRUCK",   0.78f, 0.55f, 1.30f, B::BoxTruck, R::Trucks,  700.f}, // slow, and steady
        {"MINI",        0.80f, 1.25f, 1.20f, B::Hatch,    R::Regular, 540.f, static_cast<uint8_t>(region::europe | region::oceania)}, // small, nippy, sticks to the road
        {"PICKUP",      0.92f, 0.98f, 1.02f, B::Pickup,   R::Trucks,  640.f, static_cast<uint8_t>(region::america | region::oceania | region::asia)}, // a workhorse
        {"COACH",       0.76f, 0.50f, 1.35f, B::Bus,      R::Trucks,  720.f}, // a bus: the slowest and steadiest of all
        {"ROADSTER",    1.04f, 1.10f, 0.90f, B::Car,      R::Sports,  600.f, static_cast<uint8_t>(region::asia | region::europe)}, // the golden convertible
        {"SUPERCAR",    1.19f, 1.00f, 0.88f, B::Racer,    R::Sports,  620.f, static_cast<uint8_t>(region::europe | region::america)}, // the fastest flat out
        {"AMBULANCE",   0.89f, 0.84f, 1.08f, B::Ambulance, R::Emergency, 650.f}, // from the hospital
        {"SCANNER",     1.16f, 1.15f, 0.98f, B::Scanner,      R::Secret,  600.f}, // black, a red light sweeping; nitro jumps
        {"TIME CAR",    1.05f, 1.05f, 1.05f, B::TimeCar,      R::Secret,  600.f}, // stainless steel; 88 mph travels in time
        {"SPY CAR",     1.10f, 1.08f, 1.06f, B::SpyCar,      R::Secret,  600.f}, // silver birch; the horn drops oil
        {"INTERCEPTOR", 1.19f, 1.20f, 0.85f, B::Interceptor,      R::Secret,  600.f}, // black, a blower on the bonnet
    };
    return models[((index % car_models) + car_models) % car_models];
}

bool body_shows_people(Body body) {
    return body == Body::Car || body == Body::Rig || body == Body::Police || body == Body::Scanner ||
           body == Body::TimeCar || body == Body::SpyCar || body == Body::Interceptor;
}

int next_owned_car(int current, uint32_t owned, int step) {
    const int dir = step < 0 ? -1 : 1;
    for (int k = 1; k <= car_models; ++k) {
        const int m = ((current + dir * k) % car_models + car_models) % car_models;
        if (owned & (1u << m)) return m;
    }
    return current;
}

bool body_has_lightbar(Body body) { return body == Body::Police || body == Body::Ambulance; }

bool body_is_tall(Body body) {
    return body == Body::Rig || body == Body::Van || body == Body::BoxTruck || body == Body::Bus ||
           body == Body::Ambulance;
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

int next_car_offered(int current, int arrived, CarRange range, int step) {
    std::vector<int> offer;
    if (car_model(arrived).range != range) offer.push_back(((arrived % car_models) + car_models) % car_models);
    for (int m = 0; m < car_models; ++m) {
        if (car_model(m).range == range) offer.push_back(m);
    }
    const int n = static_cast<int>(offer.size());
    const auto it = std::find(offer.begin(), offer.end(), ((current % car_models) + car_models) % car_models);
    const int here = it == offer.end() ? 0 : static_cast<int>(it - offer.begin());
    return offer[static_cast<size_t>((((here + (step < 0 ? -1 : 1)) % n) + n) % n)];
}

uint8_t region_of(const std::string& country) {
    if (country == "USA" || country == "BRAZIL") return region::america;
    if (country == "JAPAN" || country == "KOREA" || country == "INDIA") return region::asia;
    if (country == "AUSTRALIA") return region::oceania;
    if (country == "EGYPT" || country == "KENYA") return region::africa;
    return region::europe;
}

int next_car_offered(int current, int arrived, CarRange range, int step, uint8_t regions) {
    int local = 0;
    for (int m = 0; m < car_models; ++m) local += car_model(m).range == range && (car_model(m).regions & regions);
    if (local < 2) return next_car_offered(current, arrived, range, step);
    std::vector<int> offer;
    const int came = ((arrived % car_models) + car_models) % car_models;
    const bool came_local = car_model(came).range == range && (car_model(came).regions & regions);
    if (!came_local) offer.push_back(came);
    for (int m = 0; m < car_models; ++m) {
        if (car_model(m).range == range && (car_model(m).regions & regions)) offer.push_back(m);
    }
    const int n = static_cast<int>(offer.size());
    const auto it = std::find(offer.begin(), offer.end(), ((current % car_models) + car_models) % car_models);
    const int here = it == offer.end() ? 0 : static_cast<int>(it - offer.begin());
    return offer[static_cast<size_t>((((here + (step < 0 ? -1 : 1)) % n) + n) % n)];
}

float rival_speed(float cruise, float max_speed, float gap) {
    const bool racing = gap > -rival_race_range / 4.f && gap < rival_race_range;
    return racing ? std::max(cruise, rival_race_speed * max_speed) : cruise;
}

} // namespace racer
