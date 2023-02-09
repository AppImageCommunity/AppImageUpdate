#include "GithubReleasesZsyncUpdateInformation.h"

namespace appimage::update::updateinformation {

    GithubReleasesUpdateInformation::GithubReleasesUpdateInformation(
        const std::vector<std::string>& updateInformationComponents) :
        AbstractUpdateInformation(updateInformationComponents, ZSYNC_GITHUB_RELEASES)
    {
        // validation
        assertParameterCount(_updateInformationComponents, 5);
    }

    std::string GithubReleasesUpdateInformation::buildUrl(const StatusMessageCallback& issueStatusMessage) const {
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

        nlohmann::json json;

        try {
            json = nlohmann::json::parse(response.text);
        } catch (const std::exception& e) {
            throw UpdateInformationError(std::string("Failed to parse GitHub response: ") + e.what());
        }
        // continue only if request worked
        if (response.error.code != cpr::ErrorCode::OK || response.status_code < 200 || response.status_code >= 300) {
            std::ostringstream oss;
            oss << "GitHub API request failed: HTTP status " << std::to_string(response.status_code)
                << ", CURL error: " << response.error.message;
            throw UpdateInformationError(oss.str());
        }

        // not ideal, but allows for returning a match for the entire line
        auto pattern = "*" + filename;

        const auto& assets = json["assets"];

        if (assets.empty()) {
            std::ostringstream oss;
            oss << "Could not find any artifacts in release data. "
                << "Please contact the author of the AppImage and tell them the files are missing "
                <<  "on the releases page.";
            throw UpdateInformationError(oss.str());
        }

        std::vector<std::string> matchingUrls;

        for (const auto& asset : assets) {
            const auto browserDownloadUrl = asset["browser_download_url"].get<std::string>();

            if (fnmatch(pattern.c_str(), browserDownloadUrl.c_str(), 0) == 0) {
                matchingUrls.emplace_back(browserDownloadUrl);
            }
        }

        if (matchingUrls.empty()) {
            std::ostringstream oss;
            oss << "None of the artifacts matched the pattern in the update information. "
                << "The pattern is most likely invalid, e.g., due to changes in the filenames of "
                << "the AppImages. Please contact the author of the AppImage and ask them to "
                << "revise the update information.";
            throw UpdateInformationError(oss.str());
        }

        // this _should_ ensure the first entry in the vector is the latest release in case there is more than one)
        // (this of course depends on the stability of the naming pattern used by the AppImage vendors)
        std::sort(matchingUrls.begin(), matchingUrls.end(), std::greater<>());

        return matchingUrls[0];
    }
}
