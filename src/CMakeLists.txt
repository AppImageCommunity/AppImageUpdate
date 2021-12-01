cmake_minimum_required(VERSION 3.2)

set(CMAKE_POSITION_INDEPENDENT_CODE true)

include_directories(${PROJECT_SOURCE_DIR}/lib/zsync2/include)
include_directories(${PROJECT_SOURCE_DIR}/lib/zsync2/lib/cpr/include)
include_directories(${PROJECT_SOURCE_DIR}/lib/zsync2/lib/hashlib/include)

# add support for sanitizers
set(CMAKE_MODULE_PATH "${AppImageUpdate_SOURCE_DIR}/lib/sanitizers-cmake/cmake" ${CMAKE_MODULE_PATH})
find_package(Sanitizers REQUIRED)

# required to link to correct thread library
find_package(Threads REQUIRED)
find_package(X11 REQUIRED)

if (NOT ${X11_Xpm_FOUND})
    message(FATAL_ERROR "libxpm could not be found!")
else ()
    message(STATUS "Found libxpm: ${X11_Xpm_LIB}")
endif ()

# expose version data as compiler definition
add_definitions("-DAPPIMAGEUPDATE_VERSION=\"${VERSION}\"")
add_definitions("-DAPPIMAGEUPDATE_GIT_COMMIT=\"${GIT_COMMIT}\"")
add_definitions("-DBUILD_DATE=\"${DATE}\"")
add_definitions("-DBUILD_NUMBER=\"${BUILD_NUMBER}\"")

set(
    LIB_SOURCES
    util.h
    updater.cpp
    update_methods/pling_v1_zsync.cpp
)

# core library
add_library(libappimageupdate SHARED ${LIB_SOURCES})
add_library(libappimageupdate_static STATIC ${LIB_SOURCES})

# link thread libraries
target_link_libraries(libappimageupdate PRIVATE libappimage_shared libzsync2 cpr Threads::Threads)
target_link_libraries(libappimageupdate_static PRIVATE libappimage_shared libzsync2_static cpr Threads::Threads)

# prevent linked libraries from being passed when libappimageupdate_static is linked
# set_target_properties(libappimageupdate_static PROPERTIES INTERFACE_LINK_LIBRARIES "")

# include directories, publicly
target_include_directories(libappimageupdate PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include> $<INSTALL_INTERFACE:include>)
target_include_directories(libappimageupdate_static PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include> $<INSTALL_INTERFACE:include>)


set_target_properties(
    libappimageupdate libappimageupdate_static

    PROPERTIES
    # set public headers
    PUBLIC_HEADER ${PROJECT_SOURCE_DIR}/include/appimage/update.h
    # since the target is called libsomething, one doesn't need CMake's additional lib prefix
    OUTPUT_NAME "appimageupdate"
    # rpath
    INSTALL_RPATH "\$ORIGIN"
)


# CLI application
add_executable(appimageupdatetool main.cpp)
# link to core lib
target_link_libraries(appimageupdatetool libappimageupdate args)

# set up rpath
set_target_properties(appimageupdatetool PROPERTIES INSTALL_RPATH "\$ORIGIN/../lib")

# install targets
install(
    TARGETS libappimageupdate appimageupdatetool
    EXPORT AppImageUpdateTargets
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT APPIMAGEUPDATETOOL
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT LIBAPPIMAGEUPDATE
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT LIBAPPIMAGEUPDATE
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/appimage COMPONENT LIBAPPIMAGEUPDATE-DEV
)

# separate libappimageupdate_static to avoid it from being exported, there are issues with the dependencies
install(
    TARGETS libappimageupdate_static
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT APPIMAGEUPDATETOOL
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT LIBAPPIMAGEUPDATE
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT LIBAPPIMAGEUPDATE
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/appimage COMPONENT LIBAPPIMAGEUPDATE-DEV
)

# include Qt UI
set(BUILD_QT_UI OFF CACHE BOOL "Build Qt UI (widget library and demo application)")
if (BUILD_QT_UI)
    add_subdirectory(qt-ui)
endif ()

# Export targets
# https://cmake.org/cmake/help/latest/guide/importing-exporting/index.html#exporting-targets
install(
    EXPORT AppImageUpdateTargets
    FILE AppImageUpdateTargets.cmake
    NAMESPACE AppImageUpdate::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/AppImageUpdate
)

export(
    EXPORT AppImageUpdateTargets
    FILE "${CMAKE_CURRENT_BINARY_DIR}/cmake/AppImageUpdateTargets.cmake"
    NAMESPACE AppImageUpdate::
)

# CMake Package Setup
# https://cmake.org/cmake/help/latest/module/CMakePackageConfigHelpers.html#cmakepackageconfighelpers

include(CMakePackageConfigHelpers)
configure_package_config_file(
    ${PROJECT_SOURCE_DIR}/cmake/config.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/AppImageUpdateConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/AppImageUpdate
)

write_basic_package_version_file(
    ${CMAKE_CURRENT_BINARY_DIR}/AppImageUpdateConfigVersion.cmake
    VERSION ${VERSION}
    COMPATIBILITY SameMajorVersion
)

install(
    FILES
    "${CMAKE_CURRENT_BINARY_DIR}/AppImageUpdateConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/AppImageUpdateConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/AppImageUpdate
)
