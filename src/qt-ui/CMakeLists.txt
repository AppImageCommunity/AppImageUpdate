# Qt dependencies
find_package(Qt5 REQUIRED COMPONENTS Core Widgets)

# run moc automatically when needed
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)

# library with the Qt UI classes
add_library(libappimageupdate-qt SHARED qt-updater.cpp spoiler.cpp ${PROJECT_SOURCE_DIR}/include/appimage/update/qt-ui.h)
target_link_libraries(libappimageupdate-qt libappimageupdate Qt5::Core Qt5::Widgets)
set_target_properties(libappimageupdate-qt
    PROPERTIES
    PREFIX ""
    PUBLIC_HEADER ${PROJECT_SOURCE_DIR}/include/appimage/update/qt-ui.h
)
target_include_directories(libappimageupdate-qt
    PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_compile_definitions(libappimageupdate-qt PRIVATE -DQT_UI)

# library with the Qt UI classes (for static linking, see #110)
add_library(libappimageupdate-qt_static STATIC qt-updater.cpp spoiler.cpp ${PROJECT_SOURCE_DIR}/include/appimage/update/qt-ui.h)
target_link_libraries(libappimageupdate-qt_static libappimageupdate Qt5::Core Qt5::Widgets)
set_target_properties(libappimageupdate-qt_static
    PROPERTIES
    PREFIX ""
    PUBLIC_HEADER ${PROJECT_SOURCE_DIR}/include/appimage/update/qt-ui.h
)
target_include_directories(libappimageupdate-qt_static
    PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_compile_definitions(libappimageupdate-qt_static PRIVATE -DQT_UI)

# Qt GUI application
add_executable(AppImageUpdate main.cpp)

if (ENABLE_SANITIZERS)
    # add support for sanitizers.  Partially broken: fix at https://github.com/arsenm/sanitizers-cmake/pull/18
    set(CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/lib/sanitizers-cmake/cmake" ${CMAKE_MODULE_PATH})
    find_package(Sanitizers REQUIRED)

    add_sanitizers(AppImageUpdate)
endif (ENABLE_SANITIZERS)

# link libraries
target_link_libraries(AppImageUpdate libappimageupdate libappimageupdate-qt)
target_compile_definitions(AppImageUpdate PRIVATE -DQT_UI)

# set up rpath
set_target_properties(libappimageupdate-qt PROPERTIES INSTALL_RPATH "\$ORIGIN")
set_target_properties(libappimageupdate-qt_static PROPERTIES INSTALL_RPATH "\$ORIGIN")
set_target_properties(AppImageUpdate PROPERTIES INSTALL_RPATH "\$ORIGIN/../lib")

# install targets
install(
    TARGETS libappimageupdate-qt libappimageupdate-qt_static AppImageUpdate
    RUNTIME DESTINATION bin COMPONENT APPIMAGEUPDATE
    LIBRARY DESTINATION lib COMPONENT LIBAPPIMAGEUPDATE-QT
    ARCHIVE DESTINATION lib COMPONENT LIBAPPIMAGEUPDATE-QT
    PUBLIC_HEADER DESTINATION include/appimage/update COMPONENT LIBAPPIMAGEUPDATE-QT-DEV
)
