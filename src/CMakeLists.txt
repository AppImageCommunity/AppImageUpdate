cmake_minimum_required(VERSION 3.2)

# required to link to correct thread library
find_package(Threads REQUIRED)

# core library
add_library(libappimageupdate ../include/appimage/update.h updater.cpp util.h)
# since the target is called libsomething, one doesn't need CMake's additional lib prefix
set_target_properties(libappimageupdate PROPERTIES PREFIX "")
# link thread libraries
target_link_libraries(libappimageupdate ${CMAKE_THREAD_LIBS_INIT} libzsync2 cpr)
# include directories, publicly
target_include_directories(libappimageupdate PUBLIC ../include)
# works for now, but should be possible without this workaround
target_include_directories(libappimageupdate PRIVATE ../lib/zsync2/lib/cpr/include)

# CLI application
add_executable(appimageupdate main.cpp)
# link to core lib
target_link_libraries(appimageupdate libappimageupdate)

# GUI application
add_executable(appimageupdategui gui_main.cpp)
# link libraries
target_link_libraries(appimageupdategui
    libappimageupdate
    fltk
    libdesktopenvironments
)
