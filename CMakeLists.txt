cmake_minimum_required(VERSION 3.20)
project(TopGun2Recomp C)

set(NESRECOMP_ROOT ${CMAKE_SOURCE_DIR}/vendor/nesrecomp)
include("${NESRECOMP_ROOT}/runner/cyc/project.cmake")

option(TOPGUN2_HEADLESS "Build without a window (automated checks only)" OFF)
set(_headless "")
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
