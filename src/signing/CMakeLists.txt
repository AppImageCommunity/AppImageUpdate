find_package(PkgConfig)

pkg_check_modules(gpgme gpgme>=1.10.0 REQUIRED IMPORTED_TARGET)

add_library(signing STATIC signaturevalidator.cpp)
target_link_libraries(signing
    PUBLIC PkgConfig::gpgme
    PRIVATE util
)
# include the complete source to force the use of project-relative include paths
target_include_directories(signing
    PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>/src
)

# won't work before project()! https://stackoverflow.com/a/39258832
if(CMAKE_SIZEOF_VOID_P EQUAL 4)
    # GPGME requires this on 32-bit systems
    # https://www.gnupg.org/documentation/manuals/gpgme/Largefile-Support-_0028LFS_0029.html
    message(WARNING "Building on 32-bit system, adding special gpgme definitions")
    target_compile_definitions(signing
        PRIVATE -D_FILE_OFFSET_BITS=64 -DLARGEFILE_SOURCE
    )
endif()
