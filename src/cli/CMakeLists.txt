# CLI application
add_executable(appimageupdatetool main.cpp)
# link to core lib
target_link_libraries(appimageupdatetool libappimageupdate util)
if(NOT USE_SYSTEM_ZSYNC2)
    target_link_libraries(appimageupdatetool ${ZSYNC2_LIBRARY_NAME})
endif()

# set up rpath
set_target_properties(appimageupdatetool PROPERTIES INSTALL_RPATH "\$ORIGIN/../${CMAKE_INSTALL_LIBDIR}")

# install target
install(
    TARGETS appimageupdatetool
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT APPIMAGEUPDATETOOL
)
