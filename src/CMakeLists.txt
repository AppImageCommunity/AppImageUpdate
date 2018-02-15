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
add_library(libappimageupdate ${CMAKE_CURRENT_SOURCE_DIR}/../include/appimage/update.h updater.cpp util.h)
# since the target is called libsomething, one doesn't need CMake's additional lib prefix
set_target_properties(libappimageupdate PROPERTIES PREFIX "")
# link thread libraries
target_link_libraries(libappimageupdate PRIVATE ${CMAKE_THREAD_LIBS_INIT} libzsync2 ${CPR_LIBRARIES})
# include directories, publicly
target_include_directories(libappimageupdate PUBLIC
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

# GUI application
# TODO: replace with target_include_directories in FLTK CMake configuration
include_directories(../lib/fltk)
include_directories(${CMAKE_BINARY_DIR}/lib/fltk)
include_directories(${X11_Xpm_INCLUDE_PATH})

add_executable(AppImageUpdate gui_main.cpp)
add_sanitizers(AppImageUpdate)

# link libraries
target_link_libraries(AppImageUpdate libappimageupdate fltk fltk_images libdesktopenvironments ${X11_Xpm_LIB} args)
target_compile_definitions(AppImageUpdate PRIVATE -DFLTK_UI)


# install targets
install(
    TARGETS libappimageupdate appimageupdatetool AppImageUpdate
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib/static
    INCLUDES DESTINATION include
)


# include Qt UI
set(BUILD_QT_UI BOOL CACHE "bla" FALSE)
if (BUILD_QT_UI)
    add_subdirectory(qt-ui)
endif()
