find_package(nlohmann_json REQUIRED)

add_library(updateinformation STATIC
    AbstractUpdateInformation.cpp
    GenericZsyncUpdateInformation.cpp
    GithubReleasesZsyncUpdateInformation.cpp
    PlingV1UpdateInformation.cpp
    updateinformation.cpp
    factory.cpp
)
# include the complete source to force the use of project-relative include paths
target_include_directories(updateinformation
    PUBLIC $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>/src
)
target_link_libraries(updateinformation
    PRIVATE util
    PRIVATE nlohmann_json::nlohmann_json
    PRIVATE ${CPR_LIBRARIES}
)
