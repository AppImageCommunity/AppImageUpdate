# "demonstration" application for signing library
# there used to be an implementation in AppImageKit, but there is no sense in maintaining two variants
add_executable(validate validate_main.cpp)
target_link_libraries(validate signing ${CMAKE_THREAD_LIBS_INIT})

# install target
install(
    TARGETS validate
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT validate
)
