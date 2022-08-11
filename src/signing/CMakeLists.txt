find_package(PkgConfig)
pkg_check_modules(gpgme gpgme>=1.10 IMPORTED_TARGET)

add_library(signing STATIC signaturevalidator.cpp)
target_link_libraries(signing
    PUBLIC PkgConfig::gpgme
    PRIVATE util
)
# include the complete source to force the use of project-relative include paths
target_include_directories(signing
    PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>/src
)

# "demonstration" application
# used to be located within AppImageKit, but there is no sense in maintaining two implementations
add_executable(validate validate_main.cpp)
target_link_libraries(validate signing)
