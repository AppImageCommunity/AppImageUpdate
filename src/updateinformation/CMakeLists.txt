find_package(nlohmann_json REQUIRED)

add_library(updateinformation STATIC
    AbstractUpdateInformation.cpp
    GenericZsyncUpdateInformation.cpp
    GithubReleasesZsyncUpdateInformation.cpp
    PlingV1UpdateInformation.cpp
    updateinformation.cpp
    factory.cpp
)
target_include_directories(updateinformation
    PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
)
target_link_libraries(updateinformation
    PRIVATE util
    PRIVATE nlohmann_json::nlohmann_json
    PRIVATE ${CPR_LIBRARIES}
)
