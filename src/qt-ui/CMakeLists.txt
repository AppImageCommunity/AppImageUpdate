# Qt dependencies
find_package(Qt5 REQUIRED COMPONENTS Core Widgets)

# run moc automatically when needed
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)

# library with the Qt UI classes
add_library(libappimageupdate-qt SHARED qt-updater.cpp spoiler.cpp ${PROJECT_SOURCE_DIR}/include/appimage/update/qt-ui.h)
target_link_libraries(libappimageupdate-qt
    PUBLIC libappimageupdate Qt5::Core Qt5::Widgets
    PRIVATE util
)
set_target_properties(libappimageupdate-qt
    PROPERTIES
    PREFIX ""
    PUBLIC_HEADER ${PROJECT_SOURCE_DIR}/include/appimage/update/qt-ui.h
)
target_include_directories(libappimageupdate-qt
    PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    PUBLIC $<INSTALL_INTERFACE:include>
)
target_compile_definitions(libappimageupdate-qt PRIVATE -DQT_UI)
set_target_properties(libappimageupdate-qt PROPERTIES INSTALL_RPATH "\$ORIGIN/")

# library with the Qt UI classes (for static linking, see #110)
add_library(libappimageupdate-qt_static STATIC qt-updater.cpp spoiler.cpp ${PROJECT_SOURCE_DIR}/include/appimage/update/qt-ui.h)
target_link_libraries(libappimageupdate-qt_static
    PUBLIC libappimageupdate Qt5::Core Qt5::Widgets
    PRIVATE util
)
set_target_properties(libappimageupdate-qt_static
    PROPERTIES
    PREFIX ""
    PUBLIC_HEADER ${PROJECT_SOURCE_DIR}/include/appimage/update/qt-ui.h
)
target_include_directories(libappimageupdate-qt_static
    PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    PUBLIC $<INSTALL_INTERFACE:include>
)
target_compile_definitions(libappimageupdate-qt_static PRIVATE -DQT_UI)
set_target_properties(libappimageupdate-qt_static PROPERTIES INSTALL_RPATH "\$ORIGIN/")

if(NOT BUILD_LIBAPPIMAGEUPDATE_ONLY)
    # Qt GUI application
    add_executable(AppImageUpdate main.cpp)
    if(NOT USE_SYSTEM_ZSYNC2)
        target_link_libraries(AppImageUpdate ${ZSYNC2_LIBRARY_NAME})
    endif()

    if(ENABLE_SANITIZERS)
        message(STATUS "Sanitizers enabled")

        # add support for sanitizers.  Partially broken: fix at https://github.com/arsenm/sanitizers-cmake/pull/18
        function(import_sanitizers)
            set(ARGS_MAIN_PROJECT OFF)
            set(ARGS_BUILD_EXAMPLE OFF)
            set(ARGS_BUILD_UNITTESTS OFF)

            include(FetchContent)

            FetchContent_Declare(args
                GIT_REPOSITORY https://github.com/arsenm/sanitizers-cmake
                GIT_TAG c3dc841
            )

            # the sanitizers CMake script incorrectly uses CMAKE_SOURCE_PATH instead of PROJECT_SOURCE_PATH to include
            # the modules, so we need to manually fetch the target dir path from FetchContent and set up the module
            # path with it
            # we use FetchContent_Populate instead of FetchContent_MakeAvailable because we do not want CMake to call
            # add_subdirectory(...) automatically
            FetchContent_Populate(args)
            FetchContent_GetProperties(args SOURCE_DIR args_source_dir)

            set(CMAKE_MODULE_PATH "${args_source_dir}/cmake;${CMAKE_MODULE_PATH}")

            find_package(Sanitizers REQUIRED)
        endfunction()

        import_sanitizers()

        add_sanitizers(AppImageUpdate)
    endif(ENABLE_SANITIZERS)

    # link libraries
    target_link_libraries(AppImageUpdate libappimageupdate libappimageupdate-qt util)
    target_compile_definitions(AppImageUpdate PRIVATE -DQT_UI)
endif()

# set up rpath
set_target_properties(libappimageupdate-qt PROPERTIES INSTALL_RPATH "\$ORIGIN")
set_target_properties(libappimageupdate-qt_static PROPERTIES INSTALL_RPATH "\$ORIGIN")
if(NOT BUILD_LIBAPPIMAGEUPDATE_ONLY)
    set_target_properties(AppImageUpdate PROPERTIES INSTALL_RPATH "\$ORIGIN/../${CMAKE_INSTALL_LIBDIR}")
endif()

# install targets
install(
    TARGETS libappimageupdate-qt
    EXPORT AppImageUpdateTargets
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT LIBAPPIMAGEUPDATE-QT
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/appimage/update COMPONENT LIBAPPIMAGEUPDATE-QT-DEV
)
install(
    TARGETS libappimageupdate-qt_static
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR} COMPONENT LIBAPPIMAGEUPDATE-QT
    PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/appimage/update COMPONENT LIBAPPIMAGEUPDATE-QT-DEV
)
if(NOT BUILD_LIBAPPIMAGEUPDATE_ONLY)
    install(
        TARGETS AppImageUpdate
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT APPIMAGEUPDATE
    )
endif()
