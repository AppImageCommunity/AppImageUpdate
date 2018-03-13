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

# Qt GUI application
add_executable(AppImageUpdate main.cpp)
add_sanitizers(AppImageUpdate)

# link libraries
target_link_libraries(AppImageUpdate libappimageupdate libappimageupdate-qt)
target_compile_definitions(AppImageUpdate PRIVATE -DQT_UI)

# install targets
install(
    TARGETS libappimageupdate-qt AppImageUpdate
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib/static
    PUBLIC_HEADER DESTINATION include/appimage/update
)
