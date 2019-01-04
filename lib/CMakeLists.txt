add_subdirectory(zsync2 EXCLUDE_FROM_ALL)

if(NOT TARGET libappimage)
    add_subdirectory(libappimage EXCLUDE_FROM_ALL)
endif()
