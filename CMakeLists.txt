cmake_minimum_required(VERSION 3.25)

project(LANCommander.QMM.MatchStatistics VERSION 1.0.0 LANGUAGES C CXX)

if(NOT WIN32 AND NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR "The plugin builds for Windows and Linux, as QMM2 runs on them.")
endif()

# The source dependencies, all git submodules under external/. Point
# LANCOMMANDER_INTERPOSER_DIR at a working copy of the Interposer to build
# against unreleased changes to it.
set(QMM_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/qmm2"
    CACHE PATH "A QMM2 source tree (for include/qmmapi.h and the game_*.h headers)")
set(QMM_SDKS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/qmm_sdks"
    CACHE PATH "A qmm_sdks tree (the games' SDK headers)")
set(LANCOMMANDER_INTERPOSER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/LANCommander.Interposer"
    CACHE PATH "A LANCommander.Interposer source tree")

foreach(required IN ITEMS "${QMM_DIR}/include/qmmapi.h" "${QMM_SDKS_DIR}/q3a/game/g_local.h"
                          "${LANCOMMANDER_INTERPOSER_DIR}/CMakeLists.txt")
    if(NOT EXISTS "${required}")
        message(FATAL_ERROR "${required} not found. Run: git submodule update --init --recursive")
    endif()
endforeach()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib/$<CONFIG>")

if(MSVC)
    # The static CRT, as the Interposer's vcpkg triplets (windows-static) use.
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# The Interposer, for LANCommander::Interposer::Embedded. Every feature of the
# embedded library is opt-in (Interposer 1.6.0 and later), and the plugin uses
# match statistics alone; the rest would only add code it never calls.
# INTERPOSER_EMBEDDED_ONLY defines nothing but the embeddable library -- not the
# Interposer's DLL, tests or plugins -- and, with voice off, skips the
# ENet/Opus/miniaudio lookups, which is why vcpkg.json does not list them.
set(INTERPOSER_EMBEDDED_ONLY   ON CACHE BOOL "" FORCE)
set(INTERPOSER_FEATURE_MATCHES ON CACHE BOOL "" FORCE)
add_subdirectory("${LANCOMMANDER_INTERPOSER_DIR}" interposer EXCLUDE_FROM_ALL)

add_subdirectory(src)

include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
