// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstdint>
#include <string>

namespace racer {

// The kinds of vehicle in the traffic.
enum class Vehicle : uint8_t {
    Car,
    Van,
    Truck,  // a box trailer, slow
    Rival,  // a rare sports car that races the player
    Police, // never in the traffic: it turns up for a chase (see police.hpp)
    Hatch,  // a small hatchback
    Pickup,
    Bus,    // city buses, coaches and school buses: slow
    Ambulance, // some on a call, lights flashing and in a hurry
    Count
};

struct VehicleInfo {
    float width;      // world units
    float min_speed;  // cruising speed range, fractions of the player's top speed
    float max_speed;
    int styles;       // colour schemes in the sprite sheet
    float share;      // fraction of the traffic
};

const VehicleInfo& vehicle_info(Vehicle kind);

// Picks a vehicle kind by the shares, for `roll` uniform in [0, 1).
Vehicle traffic_vehicle(float roll);

// A rival cruises at `cruise` until the player comes close, ahead or just
// behind within `race_range` segments; then it races at rival_race_speed
// (of the top speed `max_speed`). `gap` is the player's distance ahead of
// the rival, in segments.
constexpr float rival_race_speed = 0.97f;
constexpr float rival_race_range = 30.f;
float rival_speed(float cruise, float max_speed, float gap);

// What a car the player can drive looks like from behind: one of the
// player's own cars (a car leaning into bends, the cab-over truck) or a
// vehicle of the traffic.
enum class Body : uint8_t {
    Car,      // make_player_car(): convertible or closed
    Rig,      // make_player_truck(): the cab-over tractor
    Van,      // the traffic's van
    BoxTruck, // the traffic's box truck
    Racer,    // the rivals' sports car
    Police,   // the police car (lightbar off)
    Hatch,    // the traffic's hatchback
    Pickup,   // the traffic's pickup
    Bus,      // the traffic's bus
    Ambulance,// the traffic's ambulance
    // The movie cars, each its own body (make_movie_car()).
    Scanner,     // the talking car: long, low, a smoked red band of tail lights, a ducktail
    TimeCar,     // the time machine: brushed steel, louvres, coils and vents, the reactor on top
    SpyCar,      // the spy's: rounded sixties lines, chrome, little fins, round lamps
    Interceptor, // the road warrior's: a roof wing, fat tyres in flared arches, side pipes
};
// Where a car is sold.
// (Emergency: the ambulance, which the hospital hands out, no dealer.
// Secret: the movie cars, found by doing the right thing in the right place.)
enum class CarRange : uint8_t { Regular, Sports, Trucks, Emergency, Secret };

// Parts of the world, as a bit set: where a car is at home, and so sold.
namespace region {
constexpr uint8_t europe = 1, asia = 2, america = 4, oceania = 8, africa = 16, everywhere = 0xff;
} // namespace region
// The part of the world a country (as the zones name it) is in.
uint8_t region_of(const std::string& country);

// The cars the player can drive. The factors are of the standard car, the
// Spider: top speed, acceleration and grip; the width is in world units;
// `regions` where it is at home (its dealers sell it there).
struct CarModel {
    const char* name;
    float top_speed;
    float acceleration;
    float grip;
    Body body;
    CarRange range;
    float width;
    uint8_t regions = region::everywhere;
};
constexpr int car_models = 22;
constexpr int ambulance_model = 17;
// The movie cars (CarRange::Secret), and how they are found:
constexpr int scanner_model = 18;     // a talking car's parody: up the ramp into a black truck in the USA
constexpr int time_car_model = 19;    // a time machine's: held at 88 mph a while
constexpr int spy_car_model = 20;     // a spy's: the horn at a sports car dealer
constexpr int interceptor_model = 21; // a road warrior's: flat out through the outback at night
constexpr int taxi_model = 6; // people hail it
const CarModel& car_model(int index);

// Seen through: the people show through the rear window (or over the seats).
bool body_shows_people(Body body);
// Tall: the cockpit sits high (vans and trucks).
bool body_is_tall(Body body);
// An emergency vehicle: a lightbar on the roof, switched with the hazard
// lights' switch, and a siren.
bool body_has_lightbar(Body body);

// The next car of `range` from `current` in the direction `step` (+1 or
// -1); from a car of another range, the first (or last) of this one.
int next_car_in_range(int current, CarRange range, int step);
// The next car on offer at a dealer of `range` for someone who came in
// `arrived`: the dealer's range, and the car they came in, if it is not of
// that range, ahead of it; so it is never lost by looking around.
int next_car_offered(int current, int arrived, CarRange range, int step);
// The same at a dealer in `regions`: only the range's cars at home there,
// unless there are fewer than two of them (then the whole range).
int next_car_offered(int current, int arrived, CarRange range, int step, uint8_t regions);

} // namespace racer
