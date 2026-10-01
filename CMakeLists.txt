cmake_minimum_required(VERSION 3.16)
project(kurvenrausch VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(SDL2 REQUIRED)

add_executable(kurvenrausch
    src/main.cpp
    src/background.cpp
    src/bitmap.cpp
    src/display.cpp
    src/font.cpp
    src/framebuffer.cpp
    src/game.cpp
    src/input.cpp
    src/placeholder.cpp
    src/road.cpp
    src/sprites.cpp
    src/track.cpp
)

target_include_directories(kurvenrausch PRIVATE include ${SDL2_INCLUDE_DIRS})
target_link_libraries(kurvenrausch PRIVATE ${SDL2_LIBRARIES})

if(MSVC)
    target_compile_options(kurvenrausch PRIVATE /W4)
else()
    target_compile_options(kurvenrausch PRIVATE -Wall -Wextra -Wpedantic -O2)
endif()
