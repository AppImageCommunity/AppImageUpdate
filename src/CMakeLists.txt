cmake_minimum_required(VERSION 3.2)

# required to link to correct thread library
find_package(Threads REQUIRED)

# core library
add_library(libappimageupdate updater.cpp)
# since the target is called libsomething, one doesn't need CMake's additional lib prefix
set_target_properties(libappimageupdate PROPERTIES PREFIX "")
# link thread libraries
target_link_libraries(libappimageupdate ${CMAKE_THREAD_LIBS_INIT} libzsync2)
# include directories, publicly
target_include_directories(libappimageupdate PUBLIC ../include)

# CLI application
add_executable(appimageupdate main.cpp)
# link to core lib
target_link_libraries(appimageupdate libappimageupdate)
