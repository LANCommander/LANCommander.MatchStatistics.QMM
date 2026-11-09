# lancommander_qmm[_x86_64]_<GAME>.dll / .so: one plugin per game, as QMM
# plugins are (stub_qmm's naming), each with the Interposer linked in whole.

# The core: game-neutral and SDK-free, so the unit tests link it on its own.
# C++17 throughout src/: the games' SDK headers predate C++20.
add_library(lc_core STATIC
    core/logparse.cpp
    core/payload.cpp
    core/standings.cpp
    core/stats.cpp
    core/text.cpp)
target_include_directories(lc_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/core")
target_compile_definitions(lc_core PRIVATE LC_PLUGIN_VERSION="${PROJECT_VERSION}")
set_target_properties(lc_core PROPERTIES CXX_STANDARD 17 POSITION_INDEPENDENT_CODE ON)

# The games, as stub_qmm's genbuild.py lists them, without the single-player
# ones (JASP, JK2SP, RTCWSP, SOF2SP, STVOYSP). The module each one is built
# with: its own in src/games/, or the generic layer.
set(LC_GAMES
    Q3A:q3a RTCWMP:rtcwmp WET:wet JK2MP:jk2mp JAMP:jamp STVOYHM:stvoyhm MOHAA:mohaa
    MOHSH:generic MOHBT:generic STEF2:generic SOF2MP:generic CODMP:generic CODUOMP:generic COD11MP:generic
    QUAKE2:generic Q2R:generic SIN:generic SOF:generic KINGPIN:generic)

# Which games have servers for which builds (genbuild.py's no_x64/no_x86/no_linux).
set(LC_GAMES_X64 Q3A RTCWMP WET JK2MP JAMP QUAKE2 Q2R)
set(LC_GAMES_NO_X86 Q2R)
set(LC_GAMES_NO_LINUX Q2R)

set(LC_ONLY_GAMES "" CACHE STRING "Build only these games (semicolon-separated codes); empty builds all")

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(LC_ARCH_SUFFIX "_x86_64")
else()
    set(LC_ARCH_SUFFIX "")
endif()

set(LC_BUILT_GAMES "")

function(lc_add_game game module)
    if(LC_ONLY_GAMES AND NOT game IN_LIST LC_ONLY_GAMES)
        return()
    endif()
    if(CMAKE_SIZEOF_VOID_P EQUAL 8 AND NOT game IN_LIST LC_GAMES_X64)
        return()
    endif()
    if(CMAKE_SIZEOF_VOID_P EQUAL 4 AND game IN_LIST LC_GAMES_NO_X86)
        return()
    endif()
    if(NOT WIN32 AND game IN_LIST LC_GAMES_NO_LINUX)
        return()
    endif()

    set(target "lancommander_qmm${LC_ARCH_SUFFIX}_${game}")

    add_library(${target} SHARED
        plugin/main.cpp
        core/interposer.cpp
        games/${module}.cpp)

    set_target_properties(${target} PROPERTIES
        PREFIX                    ""
        CXX_STANDARD              17
        C_VISIBILITY_PRESET       hidden
        CXX_VISIBILITY_PRESET     hidden
        VISIBILITY_INLINES_HIDDEN ON)

    target_include_directories(${target} SYSTEM PRIVATE "${QMM_SDKS_DIR}" "${QMM_DIR}/include")
    target_compile_definitions(${target} PRIVATE
        GAME_${game}
        GAME_STR="${game}"
        LC_PLUGIN_VERSION="${PROJECT_VERSION}")

    target_link_libraries(${target} PRIVATE lc_core LANCommander::Interposer::Embedded)

    if(WIN32)
        target_compile_definitions(${target} PRIVATE WIN32 _WINDOWS _CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_DEPRECATE)
        target_compile_options(${target} PRIVATE /W3 /EHsc /external:W0)
        target_link_options(${target} PRIVATE /DEBUG "$<$<CONFIG:Release>:/OPT:REF;/OPT:ICF>")
    else()
        target_compile_options(${target} PRIVATE -Wall -Wno-unused -fno-strict-aliasing)

        # As the Interposer's own .so: the C++ runtime linked in and hidden, and
        # the dynamic symbol table QMM_* plus Interposer* (exports.map says why).
        target_link_options(${target} PRIVATE
            -static-libstdc++ -static-libgcc
            "LINKER:--exclude-libs,ALL"
            "LINKER:--no-undefined"
            "LINKER:--version-script=${CMAKE_CURRENT_SOURCE_DIR}/plugin/exports.map")
        set_property(TARGET ${target} APPEND PROPERTY LINK_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/plugin/exports.map")
        target_link_libraries(${target} PRIVATE dl)
    endif()

    install(TARGETS ${target}
        RUNTIME DESTINATION "qmmaddons/lancommander"
        LIBRARY DESTINATION "qmmaddons/lancommander")

    set(LC_BUILT_GAMES ${LC_BUILT_GAMES} ${game} PARENT_SCOPE)
endfunction()

foreach(entry IN LISTS LC_GAMES)
    string(REPLACE ":" ";" parts "${entry}")
    list(GET parts 0 game)
    list(GET parts 1 module)
    lc_add_game(${game} ${module})
endforeach()

message(STATUS "Plugins: ${LC_BUILT_GAMES}")
