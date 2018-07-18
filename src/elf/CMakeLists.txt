cmake_minimum_required(VERSION 3.2)

# ELF library
# extracted from AppImageKit source code

# library requires C99
set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)

include(${PROJECT_SOURCE_DIR}/cmake/DownloadProject.cmake)

download_project(
    PROJ AppImageKit
    PREFIX "${CMAKE_CURRENT_BINARY_DIR}"
    GIT_REPOSITORY https://github.com/AppImage/AppImageKit
    GIT_TAG appimagetool/master
    UPDATE_DISCONNECTED 1
)

set(SOURCE_DIR "${AppImageKit_SOURCE_DIR}/src/")


add_library(
    elf
    STATIC
    "${SOURCE_DIR}/light_elf.h" "${SOURCE_DIR}/light_byteswap.h" "${SOURCE_DIR}/elf.c"
    "${SOURCE_DIR}/getsection.c" "${AppImageKit_SOURCE_DIR}/include/appimage/appimage.h"
)

target_include_directories(elf PUBLIC
    $<BUILD_INTERFACE:${AppImageKit_SOURCE_DIR}/include/>
)
