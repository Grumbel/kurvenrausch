# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

cmake_minimum_required(VERSION 3.16)
project(kurvenrausch VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(SDL2 REQUIRED)

# Everything except main() lives in a library so the tests can link it too.
add_library(kurvenrausch_core STATIC
    src/audio.cpp
    src/background.cpp
    src/bitmap.cpp
    src/display.cpp
    src/driving.cpp
    src/font.cpp
    src/framebuffer.cpp
    src/game.cpp
    src/hud.cpp
    src/input.cpp
    src/road.cpp
    src/sprites.cpp
    src/synth.cpp
    src/track.cpp
    src/weather.cpp
)
target_include_directories(kurvenrausch_core PUBLIC include ${SDL2_INCLUDE_DIRS})
target_link_libraries(kurvenrausch_core PUBLIC ${SDL2_LIBRARIES})

add_executable(kurvenrausch src/main.cpp)
target_link_libraries(kurvenrausch PRIVATE kurvenrausch_core)

include(CTest)
if(BUILD_TESTING)
    add_executable(kurvenrausch_tests tests/tests.cpp)
    target_link_libraries(kurvenrausch_tests PRIVATE kurvenrausch_core)
    add_test(NAME unit COMMAND kurvenrausch_tests)
endif()

foreach(target kurvenrausch_core kurvenrausch kurvenrausch_tests)
    if(TARGET ${target})
        if(MSVC)
            target_compile_options(${target} PRIVATE /W4)
        else()
            target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -O2)
        endif()
    endif()
endforeach()
