// SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "types.hpp"

namespace racer {

// How a head looks from behind.
enum class HeadStyle : uint8_t {
    Short,  // short hair
    Long,   // long hair
    Bun,    // hair up in a bun
    Cap,    // a cap over short hair
    Helmet, // a racing helmet
    Mohawk, // a shaved head with a crest
    Bald,
    Dog,    // a dog, with floppy ears
    Wild,   // fluffy white hair out in every direction (Einstein / Doc Brown)
    None,   // nobody: an empty seat
};

// Someone in the player's car: the driver or the passenger.
struct Person {
    const char* name;
    HeadStyle style;
    Color hair_dark, hair, hair_light; // or fur
    Color accent;                      // cap, helmet or crest
    Color skin;                        // also the arm that waves
};

// The drivers (the hospital swaps them) and the passengers: the first
// motel_passengers are the motel's to choose from, the last of them nobody
// (an empty seat); after them come the fares, people who hail a taxi.
constexpr int drivers = 6;
constexpr int motel_passengers = 6;
constexpr int nobody = motel_passengers - 1;
constexpr int fares = 4;
constexpr int passengers = motel_passengers + fares;
constexpr int first_fare = motel_passengers;
const Person& driver(int index);
const Person& passenger(int index);

} // namespace racer
