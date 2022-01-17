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

add_subdirectory(util)

if(USE_SYSTEM_LIBAPPIMAGE)
    set(LIBAPPIMAGE_LINK_TYPE PUBLIC)
else()
    set(LIBAPPIMAGE_LINK_TYPE PRIVATE)
endif()

if(USE_SYSTEM_ZSYNC2)
    set(ZSYNC2_LIBRARY_NAME libzsync2)
    set(ZSYNC2_LINK_TYPE PUBLIC)
else()
    set(ZSYNC2_LIBRARY_NAME libzsync2_static)
    set(ZSYNC2_LINK_TYPE PRIVATE)
endif()

# core library
add_library(libappimageupdate SHARED
    ${PROJECT_SOURCE_DIR}/include/appimage/update.h
    updater.cpp
    appimage.h
)
# since the target is called libsomething, one doesn't need CMake's additional lib prefix
set_target_properties(libappimageupdate
    PROPERTIES
    PREFIX ""
    PUBLIC_HEADER ${PROJECT_SOURCE_DIR}/include/appimage/update.h
)
# link thread libraries
target_link_libraries(libappimageupdate
    ${LIBAPPIMAGE_LINK_TYPE} libappimage_shared
    PRIVATE ${CMAKE_THREAD_LIBS_INIT}
    PRIVATE ${CPR_LIBRARIES}
    PRIVATE util
    ${ZSYNC2_LINK_TYPE} ${ZSYNC2_LIBRARY_NAME}
)
# include directories, publicly
target_include_directories(libappimageupdate PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
set_target_properties(libappimageupdate PROPERTIES INSTALL_RPATH "\$ORIGIN/")


# core library
add_library(libappimageupdate_static STATIC
    ${PROJECT_SOURCE_DIR}/include/appimage/update.h
    updater.cpp
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
    ${LIBAPPIMAGE_LINK_TYPE} libappimage_shared
    PRIVATE ${CMAKE_THREAD_LIBS_INIT}
    PRIVATE ${CPR_LIBRARIES}
    PRIVATE util
    ${ZSYNC2_LINK_TYPE} ${ZSYNC2_LIBRARY_NAME}
)
# include directories, publicly
target_include_directories(libappimageupdate_static PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)


# CLI application
add_executable(appimageupdatetool main.cpp)
# link to core lib
target_link_libraries(appimageupdatetool libappimageupdate util)
if(NOT USE_SYSTEM_ZSYNC2)
    target_link_libraries(appimageupdatetool ${ZSYNC2_LIBRARY_NAME})
endif()

# set up rpath
set_target_properties(libappimageupdate libappimageupdate_static PROPERTIES INSTALL_RPATH "\$ORIGIN")
set_target_properties(appimageupdatetool PROPERTIES INSTALL_RPATH "\$ORIGIN/../${CMAKE_INSTALL_LIBDIR}")

# install targets
install(
    TARGETS libappimageupdate
    EXPORT AppImageUpdateTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT LIBAPPIMAGEUPDATE
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/appimage COMPONENT LIBAPPIMAGEUPDATE-DEV
)
install(
    TARGETS libappimageupdate_static
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT LIBAPPIMAGEUPDATE
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/appimage COMPONENT LIBAPPIMAGEUPDATE-DEV
)
install(
    TARGETS appimageupdatetool
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT APPIMAGEUPDATETOOL
)


# include Qt UI
set(BUILD_QT_UI OFF CACHE BOOL "Build Qt UI (widget library and demo application)")
if(BUILD_QT_UI)
    add_subdirectory(qt-ui)
endif()
