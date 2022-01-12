cmake_minimum_required(VERSION 3.2)

set(CMAKE_POSITION_INDEPENDENT_CODE true)

# add support for sanitizers
set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/lib/sanitizers-cmake/cmake" ${CMAKE_MODULE_PATH})
find_package(Sanitizers REQUIRED)

# required to link to correct thread library
find_package(Threads REQUIRED)
find_package(X11 REQUIRED)

if(NOT ${X11_Xpm_FOUND})
    message(FATAL_ERROR "libxpm could not be found!")
else()
    message(STATUS "Found libxpm: ${X11_Xpm_LIB}")
endif()

# expose version data as compiler definition
add_definitions("-DAPPIMAGEUPDATE_VERSION=\"${VERSION}\"")
add_definitions("-DAPPIMAGEUPDATE_GIT_COMMIT=\"${GIT_COMMIT}\"")
add_definitions("-DBUILD_DATE=\"${DATE}\"")
add_definitions("-DBUILD_NUMBER=\"${BUILD_NUMBER}\"")


# core library
add_library(
    libappimageupdate
    SHARED
    ${PROJECT_SOURCE_DIR}/include/appimage/update.h updater.cpp update_methods/pling_v1_zsync.cpp
    util.h
)
# since the target is called libsomething, one doesn't need CMake's additional lib prefix
set_target_properties(libappimageupdate
    PROPERTIES
    PREFIX ""
    PUBLIC_HEADER ${PROJECT_SOURCE_DIR}/include/appimage/update.h
)
# link thread libraries
target_link_libraries(libappimageupdate
    PUBLIC ${APPIMAGE_LIB}
    PRIVATE ${CMAKE_THREAD_LIBS_INIT}
    PRIVATE libzsync2
    PRIVATE ${CPR_LIBRARIES}
)
# include directories, publicly
target_include_directories(libappimageupdate PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
    # FIXME: works for now, but should be possible without these workaround
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/lib/zsync2/lib/cpr/include>
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/lib/zsync2/include>
)


# core library
add_library(
    libappimageupdate_static
    STATIC
    ${PROJECT_SOURCE_DIR}/include/appimage/update.h updater.cpp
    util.h
)
# since the target is called libsomething, one doesn't need CMake's additional lib prefix
set_target_properties(libappimageupdate_static
    PROPERTIES
    PREFIX ""
    OUTPUT_NAME "libappimageupdate"
    PUBLIC_HEADER ${PROJECT_SOURCE_DIR}/include/appimage/update.h
)
# link thread libraries
target_link_libraries(libappimageupdate_static
    PUBLIC ${APPIMAGE_LIB}
    PRIVATE ${CMAKE_THREAD_LIBS_INIT}
    PRIVATE libzsync2
    PRIVATE ${CPR_LIBRARIES}
)
# include directories, publicly
target_include_directories(libappimageupdate_static PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
    # FIXME: works for now, but should be possible without these workaround
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/lib/zsync2/lib/cpr/include>
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/lib/zsync2/include>
)


# CLI application
add_executable(appimageupdatetool main.cpp)
# link to core lib
target_link_libraries(appimageupdatetool libappimageupdate args)

# set up rpath
set_target_properties(libappimageupdate libappimageupdate_static PROPERTIES INSTALL_RPATH "\$ORIGIN")
set_target_properties(appimageupdatetool PROPERTIES INSTALL_RPATH "\$ORIGIN/../lib")

# install targets
install(
    TARGETS libappimageupdate libappimageupdate_static appimageupdatetool
    RUNTIME DESTINATION bin COMPONENT APPIMAGEUPDATETOOL
    LIBRARY DESTINATION lib COMPONENT LIBAPPIMAGEUPDATE
    ARCHIVE DESTINATION lib COMPONENT LIBAPPIMAGEUPDATE
    PUBLIC_HEADER DESTINATION include/appimage COMPONENT LIBAPPIMAGEUPDATE-DEV
)


# include Qt UI
set(BUILD_QT_UI OFF CACHE BOOL "Build Qt UI (widget library and demo application)")
if(BUILD_QT_UI)
    add_subdirectory(qt-ui)
endif()
