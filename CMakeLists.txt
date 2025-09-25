cmake_minimum_required(VERSION 3.5)

project(AppImageUpdate)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# read Git revision ID
# WARNING: this value will be stored in the CMake cache
# to update it, you will have to reset the CMake cache
# (doesn't matter for CI builds like Travis for instance, where there's no permanent CMake cache)
execute_process(
    COMMAND git rev-parse --short HEAD
    WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_COMMIT
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

# set version and build number
set(VERSION 1-alpha)
if("$ENV{GITHUB_RUN_NUMBER}" STREQUAL "")
    set(BUILD_NUMBER "<local dev build>")
else()
    set(BUILD_NUMBER "$ENV{GITHUB_RUN_NUMBER}")
endif()

# get current date
execute_process(
    COMMAND env LC_ALL=C date -u "+%Y-%m-%d %H:%M:%S %Z"
    OUTPUT_VARIABLE DATE
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

option(ENABLE_SANITIZERS "Enable builds using sanitizers" off)

# install into proper dirs on Linux
include(GNUInstallDirs)

include(FetchContent)

option(USE_SYSTEM_ZSYNC2 "Use existing libzsync2 installed on system (or inside CMAKE_PREFIX_PATH)" OFF)
if(USE_SYSTEM_ZSYNC2)
    set(USE_SYSTEM_CPR ON)

    # we use cpr in AppImageUpdate, too
    find_package(cpr REQUIRED)
    add_library(cpr ALIAS cpr::cpr)

    # note: find_package calls must be made in the same or a parent scope
    find_package(zsync2 REQUIRED)
else()
    function(import_zsync2)
        FetchContent_Declare(zsync2
            GIT_REPOSITORY https://github.com/AppImageCommunity/zsync2
            GIT_TAG 2.0.0-alpha-1-20250925
        )
        FetchContent_MakeAvailable(zsync2)
    endfunction()

    import_zsync2()
endif()

option(USE_SYSTEM_LIBAPPIMAGE OFF "Use existing libappimage installed on system (or inside CMAKE_PREFIX_PATH)")
if(USE_SYSTEM_LIBAPPIMAGE)
    # note: find_package calls must be made in the same or a parent scope
    find_package(libappimage REQUIRED)
else()
    function(import_libappimage)
        FetchContent_Declare(libappimage
            GIT_REPOSITORY https://github.com/AppImageCommunity/libappimage
            GIT_TAG d326d15c8a9299d3f135be8282f6545b32805996
        )
        FetchContent_MakeAvailable(libappimage)
    endfunction()

    import_libappimage()
endif()

option(BUILD_QT_UI OFF "Build Qt UI (widget library and demo application)")
option(BUILD_LIBAPPIMAGEUPDATE_ONLY OFF "Skip build of appimageupdatetool and AppImageUpdate")

if(NOT BUILD_LIBAPPIMAGEUPDATE_ONLY)
    # this dependency does not come with a pkg-config file or CMake config, so we try to compile a file instead
    try_compile(HAVE_ARGAGG "${PROJECT_BINARY_DIR}/tests" "${PROJECT_SOURCE_DIR}/cmake/tests/try_argagg.cpp" OUTPUT_VARIABLE ARGAGG_OUT CMAKE_FLAGS "-DINCLUDE_DIRECTORIES=${CMAKE_REQUIRED_INCLUDES}")
    if(NOT HAVE_ARGAGG)
        message(FATAL_ERROR "${ARGAGG_OUT} argagg header not found on system, please install")
    endif()
endif()

# core source directory, contains its own CMakeLists.txt
add_subdirectory(src)

# packaging
include(${PROJECT_SOURCE_DIR}/cmake/cpack-deb.cmake)

# also, configure exported targets
include(cmake/export.cmake)
