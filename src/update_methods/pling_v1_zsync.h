#pragma once
// system
#include <string>
#include <vector>

//libraries

//local



namespace appimage::update::methods {
    /**
     * Pling is a family of services which include an AppImage store among many other things. AppImage files are
     * served from the www.appimagehub.com and www.pling.com urls. They are also available through an xml API
     * `https://api.pling.com/ocs/v1/`
     *
     * format: pling-v1-zsync|<content id>|<file name matching pattern>
     */
    class PlingV1Zsync {
    public:
        explicit PlingV1Zsync(std::vector<std::string> updateStringParts);

        static bool isUpdateStringAccepted(std::vector<std::string> updateStringParts);

        std::vector<std::string> getAvailableDownloads();

        std::string findLatestRelease(const std::vector<std::string>& downloads);

        std::string resolveZsyncUrl(const std::string& downloadUrl);

    private:
        static const char* plingContentEndpointUrl;
        std::string productId;
        std::string fileMatchingPattern;
    };
}
