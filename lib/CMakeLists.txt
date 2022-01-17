add_subdirectory(zsync2 EXCLUDE_FROM_ALL)

if(NOT USE_SYSTEM_LIBAPPIMAGE)
    add_subdirectory(libappimage EXCLUDE_FROM_ALL)
    set(LIBAPPIMAGE_LIBRARIES libappimage_shared PARENT_SCOPE)
endif()
