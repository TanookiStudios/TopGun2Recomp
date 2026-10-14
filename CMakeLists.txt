cmake_minimum_required(VERSION 3.20)
project(TopGun2Recomp C)

set(NESRECOMP_ROOT ${CMAKE_SOURCE_DIR}/vendor/nesrecomp)
include("${NESRECOMP_ROOT}/runner/cyc/project.cmake")

option(TOPGUN2_HEADLESS "Build without a window (automated checks only)" OFF)
set(_headless "")
# Cross builds (cmake/mingw-w64.cmake) run the Mac-built translator.
set(TOPGUN2_RECOMPILER "" CACHE FILEPATH "Host NESRecomp for cross builds")
if(TOPGUN2_RECOMPILER)
    list(APPEND _headless RECOMPILER "${TOPGUN2_RECOMPILER}")
endif()
if(TOPGUN2_HEADLESS)
    set(_headless HEADLESS)
endif()

# The ROM is the builder's own copy; it is git-ignored and never committed.
nesrecomp_add_cycle_game(TopGun2
    ROM "${CMAKE_SOURCE_DIR}/rom/topgun2.nes"
    GAME_CONFIG "${CMAKE_SOURCE_DIR}/game.toml"
    PLAYERS 1
    HOST_EXTRAS "${CMAKE_SOURCE_DIR}/topgun2_extras.c"
    ${_headless})

# Windows: a GUI program, so no console window opens behind the game.
if(WIN32 AND TARGET TopGun2)
    set_target_properties(TopGun2 PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

# Maddie's signature for the launcher's About page (assets/img beside the program).
if(TARGET TopGun2 AND NOT TOPGUN2_HEADLESS)
    add_custom_command(TARGET TopGun2 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:TopGun2>/assets/img
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_SOURCE_DIR}/assets/signature.png
                $<TARGET_FILE_DIR:TopGun2>/assets/img/maddie-signature.png)
endif()
