cmake_minimum_required(VERSION 3.16)
project(bitmap_racer VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(SDL2 REQUIRED)

add_executable(bitmap_racer
    src/main.cpp
    src/ecs.cpp
    src/renderer.cpp
    src/road.cpp
    src/game.cpp
    src/input.cpp
)

target_include_directories(bitmap_racer PRIVATE include ${SDL2_INCLUDE_DIRS})
target_link_libraries(bitmap_racer PRIVATE ${SDL2_LIBRARIES})

if(MSVC)
    target_compile_options(bitmap_racer PRIVATE /W4)
else()
    target_compile_options(bitmap_racer PRIVATE -Wall -Wextra -Wpedantic -O2)
endif()
