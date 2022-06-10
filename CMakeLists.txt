cmake_minimum_required(VERSION 3.0.0)
project(sokosolve VERSION 0.1.0)

set(C_STANDARD 11)
set(C_STANDARD_REQUIRED ON)

if (CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(SOKOSOLVE_STANDALONE TRUE)
endif()

option(SOKOSOLVE_BUILD_TEST "Build test executable" ${SOKOSOLVE_STANDALONE})

add_library(sokosolve STATIC solver.h solver.c hashmap.h hashmap.c bitset.h)

if(SOKOSOLVE_BUILD_TEST)
    add_executable(test test.c)
    target_link_libraries(test sokosolve)
endif()