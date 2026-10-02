# SPDX-FileCopyrightText: 2026 Ingo Ruhnke <grumbel@gmail.com>
# SPDX-License-Identifier: GPL-3.0-or-later

cmake_minimum_required(VERSION 3.16)

# Source of truth: the top-level VERSION file (e.g. "0.1.0-dev"), unless
# packaging passes the full version, e.g. Nix with .<revCount>+g<rev>
# appended for development builds: -DPROJECT_VERSION_FULL=...
if(NOT DEFINED PROJECT_VERSION_FULL)
    file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" PROJECT_VERSION_FULL LIMIT_COUNT 1)
endif()
# project() only takes a numeric version: strip -dev, +g... and the like.
string(REGEX MATCH "^[0-9]+(\\.[0-9]+)*" _project_version_numeric "${PROJECT_VERSION_FULL}")
project(kurvenrausch VERSION ${_project_version_numeric} LANGUAGES CXX)

include(GNUInstallDirs)
set(APP_ID io.github.grumbel.kurvenrausch)

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
    src/people.cpp
    src/road.cpp
    src/sprites.cpp
    src/synth.cpp
    src/track.cpp
    src/vehicles.cpp
    src/weather.cpp
)
target_include_directories(kurvenrausch_core PUBLIC include ${SDL2_INCLUDE_DIRS})
target_link_libraries(kurvenrausch_core PUBLIC ${SDL2_LIBRARIES})

add_executable(kurvenrausch src/main.cpp)
target_link_libraries(kurvenrausch PRIVATE kurvenrausch_core)
target_compile_definitions(kurvenrausch PRIVATE KURVENRAUSCH_VERSION="${PROJECT_VERSION_FULL}")

# The man page carries the version and the date of the source: that of the
# last commit, so builds are reproducible (packaging without a .git passes
# -DPROJECT_DATE=YYYY-MM-DD, as Nix does; failing both, today).
if(NOT DEFINED PROJECT_DATE)
    execute_process(COMMAND git log -1 --format=%cs
                    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                    OUTPUT_VARIABLE PROJECT_DATE OUTPUT_STRIP_TRAILING_WHITESPACE
                    RESULT_VARIABLE _git_status ERROR_QUIET)
    if(NOT _git_status EQUAL 0 OR PROJECT_DATE STREQUAL "")
        string(TIMESTAMP PROJECT_DATE "%Y-%m-%d" UTC)
    endif()
endif()
configure_file(data/kurvenrausch.6.in ${CMAKE_CURRENT_BINARY_DIR}/kurvenrausch.6 @ONLY)

# Desktop integration: the program, its desktop entry, AppStream metadata,
# icons (tools/make_icons.py regenerates them) and the man page.
install(TARGETS kurvenrausch RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES data/${APP_ID}.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(FILES data/${APP_ID}.metainfo.xml DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo)
install(DIRECTORY data/icons/ DESTINATION ${CMAKE_INSTALL_DATADIR}/icons)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/kurvenrausch.6 DESTINATION ${CMAKE_INSTALL_MANDIR}/man6)

include(CTest)
if(BUILD_TESTING)
    add_executable(kurvenrausch_tests tests/tests.cpp)
    target_link_libraries(kurvenrausch_tests PRIVATE kurvenrausch_core)
    add_test(NAME unit COMMAND kurvenrausch_tests)

    # The man page documents every option of --help.
    add_test(NAME man_page_options
             COMMAND ${CMAKE_COMMAND} -DBINARY=$<TARGET_FILE:kurvenrausch>
                     -DMAN=${CMAKE_CURRENT_BINARY_DIR}/kurvenrausch.6
                     -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/check_man.cmake)

    # The desktop files pass their validators, where those are installed
    # (the Nix dev shell has them).
    find_program(DESKTOP_FILE_VALIDATE desktop-file-validate)
    if(DESKTOP_FILE_VALIDATE)
        add_test(NAME desktop_entry
                 COMMAND ${DESKTOP_FILE_VALIDATE} ${CMAKE_CURRENT_SOURCE_DIR}/data/${APP_ID}.desktop)
    endif()
    find_program(APPSTREAMCLI appstreamcli)
    if(APPSTREAMCLI)
        add_test(NAME appstream_metainfo
                 COMMAND ${APPSTREAMCLI} validate --no-net --explain
                         ${CMAKE_CURRENT_SOURCE_DIR}/data/${APP_ID}.metainfo.xml)
    endif()
    find_program(MANDOC mandoc)
    if(MANDOC)
        add_test(NAME man_page_lint
                 COMMAND ${MANDOC} -T lint -W warning ${CMAKE_CURRENT_BINARY_DIR}/kurvenrausch.6)
    endif()
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
