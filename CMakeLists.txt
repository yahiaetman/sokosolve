cmake_minimum_required(VERSION 3.0.0)
project(sokosolve VERSION 0.1.0)

set(C_STANDARD 11)
set(C_STANDARD_REQUIRED ON)

if (CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(SOKOSOLVE_STANDALONE TRUE)
endif()

option(SOKOSOLVE_BUILD_TEST "Build test executable" ${SOKOSOLVE_STANDALONE})

add_library(sokosolve STATIC solver.h solver_types.h solver.c hashmap.h hashmap.c bitset.h)

if(SOKOSOLVE_BUILD_TEST)
    enable_testing()
    add_executable(tester test.c)
    target_link_libraries(tester sokosolve)
    add_test(tests_4x4 tester ${PROJECT_SOURCE_DIR}/test_cases/tests_4x4.txt)
    add_test(tests_rct tester ${PROJECT_SOURCE_DIR}/test_cases/tests_rect.txt)
    add_test(tests_7x7 tester ${PROJECT_SOURCE_DIR}/test_cases/tests_7x7.txt)
    add_test(tests_9x9 tester ${PROJECT_SOURCE_DIR}/test_cases/tests_9x9.txt)
endif()