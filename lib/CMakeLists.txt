add_subdirectory(zsync2)

if(BUILD_FLTK_UI)
    add_subdirectory(fltk)
    add_subdirectory(libdesktopenvironments)
endif()
