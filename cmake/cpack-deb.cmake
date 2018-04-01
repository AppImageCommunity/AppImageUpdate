# required for DEB-DEFAULT to work as intended
cmake_minimum_required(VERSION 3.6)

# TODO: modify dependency lists depending on settings like USE_SYSTEM_CURL
# this file has been built for the default settings

set(PROJECT_VERSION 1.0)
set(CPACK_GENERATOR "DEB")

set(CPACK_DEBIAN_PACKAGE_DEBUG ON)

# packaging only libappimageupdate for now
#set(CPACK_COMPONENTS_ALL APPIMAGEUPDATE APPIMAGEUPDATETOOL LIBAPPIMAGEUPDATE LIBAPPIMAGEUPDATE-DEV LIBAPPIMAGEUPDATE-QT LIBAPPIMAGEUPDATE-QT-DEV)
set(CPACK_COMPONENTS_ALL LIBAPPIMAGEUPDATE LIBAPPIMAGEUPDATE-DEV)

# global options
set(CPACK_PACKAGE_CONTACT "TheAssassin")
set(CPACK_PACKAGE_HOMEPAGE "https://github.com/AppImage/AppImageUpdate")
set(CPACK_PACKAGE_DESCRIPTION_FILE "${CMAKE_SOURCE_DIR}/README.md")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE.txt")

# allow building Debian packages on non-Debian systems
if(DEFINED ENV{ARCH})
    set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE $ENV{ARCH})
    if(CPACK_DEBIAN_PACKAGE_ARCHITECTURE MATCHES "i686")
        set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "i386")
    endif()
endif()

# make sure to package components separately
#set(CPACK_DEB_PACKAGE_COMPONENT ON)
set(CPACK_DEB_COMPONENT_INSTALL ON)

# override default package naming
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)

# Debian packaging global options
set(CPACK_DEBIAN_COMPRESSION_TYPE xz)

set(CPACK_DEBIAN_LIBAPPIMAGEUPDATE_PACKAGE_NAME "libappimageupdate")
set(CPACK_DEBIAN_LIBAPPIMAGEUPDATE_PACKAGE_VERSION "${APPIMAGEUPDATE_VERSION}")
set(CPACK_DEBIAN_LIBAPPIMAGEUPDATE_PACKAGE_RELEASE "git${APPIMAGEUPDATE_GIT_COMMIT}")
set(CPACK_DEBIAN_LIBAPPIMAGEUPDATE_PACKAGE_DEPENDS "libcurl3, libstdc++6, libgcc1, libc6")

set(CPACK_DEBIAN_LIBAPPIMAGEUPDATE-DEV_PACKAGE_NAME "libappimageupdate-dev")
set(CPACK_DEBIAN_LIBAPPIMAGEUPDATE-DEV_PACKAGE_VERSION "${APPIMAGEUPDATE_VERSION}")
set(CPACK_DEBIAN_LIBAPPIMAGEUPDATE-DEV_PACKAGE_RELEASE "git${APPIMAGEUPDATE_GIT_COMMIT}")
set(CPACK_DEBIAN_LIBAPPIMAGEUPDATE-DEV_PACKAGE_DEPENDS "libappimageupdate, libcurl3-gnutls-dev, libstdc++-5-dev")

# improve dependency list
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)

# TODO: insert some useful description
set(CPACK_COMPONENT_APPIMAGELAUNCHER_PACKAGE_DESCRIPTION "AppImageLauncher")

# find more suitable section for package
set(CPACK_COMPONENT_APPIMAGELAUNCHER_PACKAGE_SECTION misc)

# add postinst and prerm hooks to Debian package
set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA
    "${PROJECT_SOURCE_DIR}/cmake/debian/postinst;${PROJECT_SOURCE_DIR}/cmake/debian/prerm")

# must be the last instruction
include(CPack)
