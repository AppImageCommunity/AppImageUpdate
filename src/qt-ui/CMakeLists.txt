# Qt dependencies
find_package(Qt5 REQUIRED COMPONENTS Core Widgets)

# run moc automatically when needed
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOMOC ON)

# library with the Qt UI classes
add_library(libappimageupdate-qt qt-updater.cpp spoiler.cpp ${PROJECT_SOURCE_DIR}/include/appimage/update/qt-ui.h)
target_link_libraries(libappimageupdate-qt libappimageupdate Qt5::Core Qt5::Widgets)
target_include_directories(libappimageupdate-qt
    PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)
target_compile_definitions(libappimageupdate-qt PRIVATE -DQT_UI)

# Qt GUI application
add_executable(AppImageUpdate-Qt main.cpp)
add_sanitizers(AppImageUpdate-Qt)

# link libraries
target_link_libraries(AppImageUpdate-Qt libappimageupdate libappimageupdate-qt)
target_compile_definitions(AppImageUpdate-Qt PRIVATE -DQT_UI)

# install targets
install(
    TARGETS libappimageupdate-qt AppImageUpdate-Qt
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib/static
    INCLUDES DESTINATION include
)
