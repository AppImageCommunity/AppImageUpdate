if(USE_SYSTEM_LIBAPPIMAGE)
    set(LIBAPPIMAGE_LINK_TYPE PUBLIC)
else()
    set(LIBAPPIMAGE_LINK_TYPE PRIVATE)
endif()

add_library(appimage STATIC appimage.cpp)
# include the complete source to force the use of project-relative include paths
target_include_directories(appimage
    PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>/src
)
target_link_libraries(appimage
    ${LIBAPPIMAGE_LINK_TYPE} libappimage_shared
    ${ZSYNC2_LINK_TYPE} ${ZSYNC2_LIBRARY_NAME}
    PRIVATE updateinformation
    PRIVATE util
)
