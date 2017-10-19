cmake_minimum_required(VERSION 3.2)

project(appimageupdate)

# enable C++11
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# include external libraries
add_subdirectory(lib)

# core source directory, contains its own CMakeLists.txt
add_subdirectory(src)
