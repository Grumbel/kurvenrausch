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

if(EMSCRIPTEN)
    # WebAssembly. SDL2 is either a static build under SDL2_ROOT (headers
    # and libSDL2.a, as nix/wasm.nix makes it, offline) or Emscripten's own
    # port, which it downloads on first use. Emscripten's toolchain limits
    # find_*() to its sysroot, so SDL2_ROOT is used directly.
    if(SDL2_ROOT)
        if(NOT EXISTS "${SDL2_ROOT}/lib/libSDL2.a" OR NOT EXISTS "${SDL2_ROOT}/include/SDL2/SDL.h")
            message(FATAL_ERROR "SDL2_ROOT=${SDL2_ROOT} has no lib/libSDL2.a or include/SDL2/SDL.h")
        endif()
        set(SDL2_INCLUDE_DIRS "${SDL2_ROOT}/include")
        set(SDL2_LIBRARIES "${SDL2_ROOT}/lib/libSDL2.a")
    else()
        add_compile_options(-sUSE_SDL=2)
        add_link_options(-sUSE_SDL=2)
        set(SDL2_INCLUDE_DIRS "")
        set(SDL2_LIBRARIES "")
    endif()
    set(BUILD_TESTING OFF CACHE BOOL "No tests in the WebAssembly build" FORCE)
elseif(NOT SDL2_LIBRARIES)
    # (Unless given: a cross build whose SDL2 CMake files name the target's
    # own paths passes SDL2_INCLUDE_DIRS and SDL2_LIBRARIES, see nix/r36s.nix.)
    find_package(SDL2 REQUIRED)
endif()

# Everything except main() lives in a library so the tests can link it too.
add_library(kurvenrausch_core STATIC
    src/audio.cpp
    src/background.cpp
    src/bitmap.cpp
    src/climate.cpp
    src/daylight.cpp
    src/display.cpp
    src/driving.cpp
    src/font.cpp
    src/framebuffer.cpp
    src/game.cpp
    src/hud.cpp
    src/input.cpp
    src/menu.cpp
    src/music.cpp
    src/overlay.cpp
    src/people.cpp
    src/police.cpp
    src/road.cpp
    src/sprites.cpp
    src/state.cpp
    src/touch.cpp
    src/synth.cpp
    src/track.cpp
    src/vehicles.cpp
    src/views.cpp
    src/weather.cpp
)
target_include_directories(kurvenrausch_core PUBLIC include ${SDL2_INCLUDE_DIRS})
target_link_libraries(kurvenrausch_core PUBLIC ${SDL2_LIBRARIES})

add_executable(kurvenrausch src/main.cpp)
target_link_libraries(kurvenrausch PRIVATE kurvenrausch_core)
if(WIN32)
    # A windowed program (no console) with the game's icon; the GCC runtime
    # linked in, so only SDL2.dll (and MinGW's thread library) go along.
    enable_language(RC)
    target_compile_definitions(kurvenrausch_core PUBLIC _WIN32_WINNT=0x0601) # Windows 7 and later
    set(KURVENRAUSCH_ICO ${CMAKE_CURRENT_SOURCE_DIR}/data/icons/kurvenrausch.ico)
    configure_file(data/kurvenrausch.rc.in ${CMAKE_CURRENT_BINARY_DIR}/kurvenrausch.rc @ONLY)
    target_sources(kurvenrausch PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/kurvenrausch.rc)
    set_target_properties(kurvenrausch PROPERTIES WIN32_EXECUTABLE TRUE)
    if(MINGW)
        target_link_options(kurvenrausch PRIVATE -static-libgcc -static-libstdc++)
    endif()
endif()
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

if(EMSCRIPTEN)
    # The web page: kurvenrausch.html (also as index.html) with its .js and
    # .wasm, installed at the top of the prefix, ready to serve as a site.
    # The shell carries the version and the source revision (pass
    # -DKURVENRAUSCH_GIT_REV=..., as Nix does).
    if(NOT DEFINED KURVENRAUSCH_GIT_REV)
        execute_process(COMMAND git rev-parse --short HEAD
                        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                        OUTPUT_VARIABLE KURVENRAUSCH_GIT_REV OUTPUT_STRIP_TRAILING_WHITESPACE
                        RESULT_VARIABLE _git_status ERROR_QUIET)
        if(NOT _git_status EQUAL 0 OR KURVENRAUSCH_GIT_REV STREQUAL "")
            set(KURVENRAUSCH_GIT_REV "unknown")
        endif()
    endif()
    set(KURVENRAUSCH_SOURCE_URL "https://github.com/Grumbel/kurvenrausch")
    configure_file(mk/wasm/shell.html ${CMAKE_CURRENT_BINARY_DIR}/shell.html @ONLY)

    set_target_properties(kurvenrausch PROPERTIES SUFFIX ".html")
    target_link_options(kurvenrausch PRIVATE
        --shell-file ${CMAKE_CURRENT_BINARY_DIR}/shell.html
        -sALLOW_MEMORY_GROWTH=1
        # Emscripten 6 makes growable memory a resizable ArrayBuffer, which
        # TextDecoder rejects (a black page at startup); grow the classic way.
        -sGROWABLE_ARRAYBUFFERS=0
        # The page keeps the state directory in IndexedDB (IDBFS).
        -sFORCE_FILESYSTEM=1
        -lidbfs.js
        -sEXPORTED_RUNTIME_METHODS=FS,addRunDependency,removeRunDependency
        -sEXPORTED_FUNCTIONS=_main,_kurvenrausch_pause
        -sENVIRONMENT=web)
    set_property(TARGET kurvenrausch APPEND PROPERTY LINK_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/mk/wasm/shell.html)

    set(_web ${CMAKE_CURRENT_BINARY_DIR}/kurvenrausch)
    install(FILES ${_web}.html ${_web}.js ${_web}.wasm DESTINATION .)
    install(FILES ${_web}.html DESTINATION . RENAME index.html)
else()
    # Desktop integration: the program, its desktop entry, AppStream metadata,
    # icons (tools/make_icons.py regenerates them) and the man page.
    install(TARGETS kurvenrausch RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    install(FILES data/${APP_ID}.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
    install(FILES data/${APP_ID}.metainfo.xml DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo)
    install(DIRECTORY data/icons/hicolor DESTINATION ${CMAKE_INSTALL_DATADIR}/icons)
    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/kurvenrausch.6 DESTINATION ${CMAKE_INSTALL_MANDIR}/man6)
endif()

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
