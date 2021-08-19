cmake_minimum_required(VERSION 3.0.0)
project(sokosolve VERSION 0.1.0)

set(C_STANDARD 11)
set(C_STANDARD_REQUIRED ON)

add_executable(test test.c solver.c hashmap.c)