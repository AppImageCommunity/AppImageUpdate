find_package(PkgConfig)

pkg_check_modules(gpgme gpgme>=1.10.0 REQUIRED IMPORTED_TARGET)

add_library(signing STATIC signaturevalidator.cpp)
target_link_libraries(signing
    PRIVATE PkgConfig::gpgme
    PRIVATE util
)
# include the complete source to force the use of project-relative include paths
target_include_directories(signing
    PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>/src
)

# GPGME requires this on 32-bit systems
# https://www.gnupg.org/documentation/manuals/gpgme/Largefile-Support-_0028LFS_0029.html
# probably an oversight by the distribution, it doesn't list these flags in gpgme.pc
message(STATUS "Enabling largefile support for signing library")
target_compile_definitions(signing
    PRIVATE -D_FILE_OFFSET_BITS=64 -DLARGEFILE_SOURCE
)
