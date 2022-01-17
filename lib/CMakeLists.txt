if(NOT USE_SYSTEM_ZSYNC2)
    add_subdirectory(zsync2 EXCLUDE_FROM_ALL)
endif()

if(NOT USE_SYSTEM_LIBAPPIMAGE)
    add_subdirectory(libappimage EXCLUDE_FROM_ALL)
endif()
