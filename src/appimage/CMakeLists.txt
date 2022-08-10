if(USE_SYSTEM_LIBAPPIMAGE)
    set(LIBAPPIMAGE_LINK_TYPE PUBLIC)
else()
    set(LIBAPPIMAGE_LINK_TYPE PRIVATE)
endif()

add_library(appimage STATIC appimage.cpp)
target_include_directories(appimage
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)
target_link_libraries(appimage
    ${LIBAPPIMAGE_LINK_TYPE} libappimage_shared
    ${ZSYNC2_LINK_TYPE} ${ZSYNC2_LIBRARY_NAME}
    PRIVATE updateinformation
    PRIVATE util
)
