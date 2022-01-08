# INTERFACE library with source files requires CMake 3.19
cmake_minimum_required(VERSION 3.19)

add_library(util INTERFACE
    util.h
)
target_include_directories(util INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})
