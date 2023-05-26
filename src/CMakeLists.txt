set(CMAKE_POSITION_INDEPENDENT_CODE true)

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

if(USE_SYSTEM_ZSYNC2)
    set(ZSYNC2_LIBRARY_NAME libzsync2)
    set(ZSYNC2_LINK_TYPE PUBLIC)
else()
    set(ZSYNC2_LIBRARY_NAME libzsync2_static)
    set(ZSYNC2_LINK_TYPE PRIVATE)
endif()

add_subdirectory(util)
add_subdirectory(updateinformation)
add_subdirectory(signing)
add_subdirectory(updater)

if(NOT BUILD_LIBAPPIMAGEUPDATE_ONLY)
    add_subdirectory(cli)
    add_subdirectory(validate)
endif()

# include Qt UI
if(BUILD_QT_UI)
    add_subdirectory(qt-ui)
endif()
