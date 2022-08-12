find_package(nlohmann_json REQUIRED)

# compatibility with Ubuntu 18.04's nlohmann-json-dev
if(TARGET nlohmann_json AND NOT TARGET nlohmann_json::nlohmann_json)
    add_library(nlohmann_json::nlohmann_json ALIAS nlohmann_json)
endif()

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
