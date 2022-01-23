#pragma once

// system headers
#include <fnmatch.h>

// local headers
#include "common.h"

namespace appimage::update::updateinformation {
    class GithubReleasesUpdateInformation : public AbstractUpdateInformation {
    public:
        explicit GithubReleasesUpdateInformation(const std::vector<std::string>& updateInformationComponents) :
            AbstractUpdateInformation(updateInformationComponents, ZSYNC_GITHUB_RELEASES)
        {
            // validation
            assertParameterCount(_updateInformationComponents, 5);
        }

    public:
        [[nodiscard]] std::string buildUrl(const StatusMessageCallback& issueStatusMessage) const override {
            auto username = _updateInformationComponents[1];
            auto repository = _updateInformationComponents[2];
            auto tag = _updateInformationComponents[3];
            auto filename = _updateInformationComponents[4];

            std::stringstream url;
            url << "https://api.github.com/repos/" << username << "/" << repository << "/releases/";

            if (tag.find("latest") != std::string::npos) {
                issueStatusMessage("Fetching latest release information from GitHub API");
                url << "latest";
            } else {
                std::ostringstream oss;
                oss << "Fetching release information for tag \"" << tag << "\" from GitHub API.";
                issueStatusMessage(oss.str());
                url << "tags/" << tag;
            }

            auto response = cpr::Get(cpr::Url{url.str()});

            // counter that will be evaluated later to give some meaningful feedback why parsing API
            // response might have failed
            int downloadUrlLines = 0;
            int matchingUrls = 0;

            std::string builtUrl;

            // continue only if request worked
            if (response.error.code != cpr::ErrorCode::OK || response.status_code < 200 || response.status_code >= 300) {
                std::ostringstream oss;
                oss << "GitHub API request failed: HTTP status " << std::to_string(response.status_code)
                    << ", CURL error: " << response.error.message;
                throw UpdateInformationError(oss.str());
            }


            // in contrary to the original implementation, instead of converting wildcards into
            // all-matching regular expressions, we have the power of fnmatch() available, a real wildcard
            // implementation
            // unfortunately, this is still hoping for GitHub's JSON API to return a pretty printed
            // response which can be parsed like this
            std::stringstream responseText(response.text);
            std::string currentLine;

            // not ideal, but allows for returning a match for the entire line
            auto pattern = "*" + filename + "*";

            // iterate through all lines to find a possible download URL and compare it to the pattern
            while (std::getline(responseText, currentLine)) {
                if (currentLine.find("browser_download_url") != std::string::npos) {
                    downloadUrlLines++;
                    if (fnmatch(pattern.c_str(), currentLine.c_str(), 0) == 0) {
                        matchingUrls++;
                        auto parts = util::split(currentLine, '"');
                        builtUrl = std::string(parts.back());
                        break;
                    }
                }
            }

            if (downloadUrlLines <= 0) {
                std::ostringstream oss;
                oss << "Could not find any artifacts in release data. "
                    << "Please contact the author of the AppImage and tell them the files are missing "
                    <<  "on the releases page.";
                throw UpdateInformationError(oss.str());
            } else if (matchingUrls <= 0) {
                std::ostringstream oss;
                oss << "None of the artifacts matched the pattern in the update information. "
                    << "The pattern is most likely invalid, e.g., due to changes in the filenames of "
                    << "the AppImages. Please contact the author of the AppImage and ask them to "
                    << "revise the update information.";
                throw UpdateInformationError(oss.str());
            } else if (builtUrl.empty()) {
                // unlike that this code will ever be reached, the other two messages should cover all
                // cases in which a ZSync URL is missing
                // if it does, however, it's most likely that GitHub's API didn't return a URL
                throw UpdateInformationError("Failed to parse GitHub's response.");
            }

            return builtUrl;
        }
    };
}
