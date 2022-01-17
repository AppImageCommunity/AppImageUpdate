add_library(updateinformation INTERFACE
    AbstractUpdateInformation.h
    GenericZsyncUpdateInformation.h
    GithubReleasesZsyncUpdateInformation.h
    PlingV1UpdateInformation.h
    common.h
    updateinformation.h
    factory.h
)
target_include_directories(updateinformation INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)
target_link_libraries(updateinformation INTERFACE util)
