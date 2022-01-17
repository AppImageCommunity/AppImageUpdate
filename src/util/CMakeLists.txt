# INTERFACE library with source files requires CMake 3.19
cmake_minimum_required(VERSION 3.19)

add_library(util STATIC
    util.cpp
    util.h
)
target_include_directories(util PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)
target_link_libraries(util PRIVATE libappimage_shared ${ZSYNC2_LIBRARY_NAME} cpr)
