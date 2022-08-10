// system headers
#include <regex>
#include <fnmatch.h>

// library headers
#include <cpr/cpr.h>

// local headers
#include "PlingV1UpdateInformation.h"

namespace appimage::update::updateinformation {
    namespace {
        const char* plingContentEndpointUrl = "https://api.pling.com/ocs/v1/content/data/";
    }


    PlingV1UpdateInformation::PlingV1UpdateInformation(const std::vector<std::string>& updateInformationComponents) :
        AbstractUpdateInformation(updateInformationComponents, ZSYNC_PLING_V1),
        _productId(updateInformationComponents[1]),
        _fileMatchingPattern(updateInformationComponents[2])
    {
        // validation
        assertParameterCount(_updateInformationComponents, 3);
    }

    std::vector<std::string> PlingV1UpdateInformation::_getAvailableDownloads() const {
        std::vector<std::string> downloads;

        cpr::Url productDetailsUrl = plingContentEndpointUrl + _productId;
        auto response = cpr::Get(productDetailsUrl);
        if (response.status_code >= 200 && response.status_code < 300) {
            std::regex urlRegex(R"((?:\<downloadlink\d+\>)(.*?)(?:<\/downloadlink\d+\>))");

            std::string text = response.text;
            std::smatch match;

            // match download link
            while (std::regex_search(text, match, urlRegex)) {
                // extract second matched group which contains the actual url
                std::string url = match[1].str();

                // apply file matching patter to the file name
                auto fileName = url.substr(url.rfind('/') + 1);
                if (fnmatch(_fileMatchingPattern.data(), fileName.data(), 0) == 0)
                    downloads.push_back(url);

                text = match.suffix();
            }
        }

        return downloads;
    }

    std::string PlingV1UpdateInformation::_findLatestRelease(const std::vector<std::string>& downloads) {
        std::string latestReleaseUrl;
        std::string latestReleaseFileName;

        for (const auto& url: downloads) {
            auto file_name = url.substr(url.rfind('/') + 1);

            // keep only the latest release
            if (file_name > latestReleaseFileName) {
                latestReleaseUrl = std::string(url);
                latestReleaseFileName = std::string(file_name);
                break;
            }
        }

        return latestReleaseUrl;
    }

    std::string PlingV1UpdateInformation::_resolveZsyncUrl(const std::string& downloadUrl) {
        // pling.com creates zsync files for every uploaded file, we just need to append .zsync
        return downloadUrl + ".zsync";
    }

    std::string PlingV1UpdateInformation::buildUrl(const StatusMessageCallback& issueStatusMessage) const {
        const auto& productId = _updateInformationComponents[1];
        const auto& fileMatchingPattern = _updateInformationComponents[2];

        const auto availableDownloads = _getAvailableDownloads();
        const auto latestReleaseUrl = _findLatestRelease(availableDownloads);
        auto zsyncUrl = _resolveZsyncUrl(latestReleaseUrl);

        return zsyncUrl;
    }
}
