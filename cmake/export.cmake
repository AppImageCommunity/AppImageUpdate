# make sure CMake won't export this package by default (valid from 3.15 onwards)
cmake_minimum_required(VERSION 3.15)
set(CMP0090 NEW)

# allow import from current build tree
export(
    TARGETS libappimageupdate
    NAMESPACE AppImageUpdate::
    FILE ${PROJECT_BINARY_DIR}/cmake/AppImageUpdateTargets.cmake
)

# allow import from install tree
# note that the targets must be install(... EXPORT zsync) in order for this to work (consider libappimageupdate a namespace)
install(
    EXPORT AppImageUpdateTargets
    DESTINATION lib/cmake/AppImageUpdate
)

include(CMakePackageConfigHelpers)
# generate the config file that is includes the exports
configure_package_config_file(
    "${CMAKE_CURRENT_LIST_DIR}/AppImageUpdateConfig.cmake.in"
    "${PROJECT_BINARY_DIR}/cmake/AppImageUpdateConfig.cmake"
    INSTALL_DESTINATION "lib/cmake/AppImageUpdate"
    NO_SET_AND_CHECK_MACRO
    NO_CHECK_REQUIRED_COMPONENTS_MACRO
)

write_basic_package_version_file(
    "${PROJECT_BINARY_DIR}/cmake/AppImageUpdateConfigVersion.cmake"
    VERSION "${VERSION}"
    COMPATIBILITY AnyNewerVersion
)

install(
    FILES
    "${PROJECT_BINARY_DIR}/cmake/AppImageUpdateConfig.cmake"
    "${PROJECT_BINARY_DIR}/cmake/AppImageUpdateConfigVersion.cmake"
    DESTINATION lib/cmake/AppImageUpdate
)
