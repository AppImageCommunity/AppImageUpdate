#pragma once

// system headers
#include <regex>

// local headers
#include "common.h"

namespace appimage::update::updateinformation {
    namespace {
        const char* plingContentEndpointUrl = "https://api.pling.com/ocs/v1/content/data/";
    }

    /**
     * Pling is a family of services which include an AppImage store among many other things. AppImage files are
     * served from the www.appimagehub.com and www.pling.com urls. They are also available through an xml API
     * `https://api.pling.com/ocs/v1/`
     *
     * format: pling-v1-zsync|<content id>|<file name matching pattern>
     */
    class PlingV1UpdateInformation : public AbstractUpdateInformation {
    private:
        std::string _fileMatchingPattern;
        std::string _productId;

    public:
        explicit PlingV1UpdateInformation(const std::vector<std::string>& updateInformationComponents) :
            AbstractUpdateInformation(updateInformationComponents, ZSYNC_PLING_V1),
            _productId(updateInformationComponents[1]),
            _fileMatchingPattern(updateInformationComponents[2])
        {
            // validation
            assertParameterCount(_updateInformationComponents, 3);
        }

    private:
        [[nodiscard]] std::vector<std::string> _getAvailableDownloads() const {
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

        static std::string _findLatestRelease(const std::vector<std::string>& downloads) {
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

        static std::string _resolveZsyncUrl(const std::string& downloadUrl) {
            // pling.com creates zsync files for every uploaded file, we just need to append .zsync
            return downloadUrl + ".zsync";
        }

        [[nodiscard]] std::string buildUrl(const StatusMessageCallback& issueStatusMessage) const override {
            const auto& productId = _updateInformationComponents[1];
            const auto& fileMatchingPattern = _updateInformationComponents[2];

            const auto availableDownloads = _getAvailableDownloads();
            const auto latestReleaseUrl = _findLatestRelease(availableDownloads);
            auto zsyncUrl = _resolveZsyncUrl(latestReleaseUrl);

            return zsyncUrl;
        }
    };
}
