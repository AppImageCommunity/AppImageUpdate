# GUI application
# TODO: replace with target_include_directories in FLTK CMake configuration
include_directories(../../lib/fltk)
include_directories(${CMAKE_BINARY_DIR}/lib/fltk)
include_directories(${X11_Xpm_INCLUDE_PATH})

add_executable(AppImageUpdate gui_main.cpp)
add_sanitizers(AppImageUpdate)

# link libraries
target_link_libraries(AppImageUpdate libappimageupdate fltk fltk_images libdesktopenvironments ${X11_Xpm_LIB} args)
target_compile_definitions(AppImageUpdate PRIVATE -DFLTK_UI)

install(
    TARGETS AppImageUpdate
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib/static
    INCLUDES DESTINATION include
)
