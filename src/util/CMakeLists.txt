# INTERFACE library with source files requires CMake 3.19
cmake_minimum_required(VERSION 3.19)

add_library(util STATIC
    util.cpp
    updatableappimage.cpp
)
# include the complete source to force the use of project-relative include paths
target_include_directories(util
    PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>/src
)
target_link_libraries(util PRIVATE libappimage_shared ${ZSYNC2_LIBRARY_NAME} cpr)
