# core library
add_library(libappimageupdate SHARED
    ${PROJECT_SOURCE_DIR}/include/appimage/update.h
    updater.cpp
)
# since the target is called libsomething, one doesn't need CMake's additional lib prefix
set_target_properties(libappimageupdate
    PROPERTIES
    PREFIX ""
    PUBLIC_HEADER ${PROJECT_SOURCE_DIR}/include/appimage/update.h
)
# link thread libraries
target_link_libraries(libappimageupdate
    PRIVATE ${CMAKE_THREAD_LIBS_INIT}
    PRIVATE ${CPR_LIBRARIES}
    PRIVATE util
    PRIVATE updateinformation
    PRIVATE signing
    ${ZSYNC2_LINK_TYPE} ${ZSYNC2_LIBRARY_NAME}
)
# include directories, publicly
target_include_directories(libappimageupdate
    PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    PUBLIC $<INSTALL_INTERFACE:include>
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
    PRIVATE ${CMAKE_THREAD_LIBS_INIT}
    PRIVATE ${CPR_LIBRARIES}
    PRIVATE util
    PRIVATE updateinformation
    PRIVATE signing
    ${ZSYNC2_LINK_TYPE} ${ZSYNC2_LIBRARY_NAME}
)
# include directories, publicly
target_include_directories(libappimageupdate_static
    PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    PUBLIC $<INSTALL_INTERFACE:include>
)
# set up rpath
set_target_properties(libappimageupdate libappimageupdate_static PROPERTIES INSTALL_RPATH "\$ORIGIN")

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
