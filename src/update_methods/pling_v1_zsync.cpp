// system
#include <fnmatch.h>
#include <regex>

//libraries
#include <cpr/cpr.h>

//local
#include "pling_v1_zsync.h"

const char* appimage::update::methods::PlingV1Zsync::plingContentEndpointUrl = "https://api.pling.com/ocs/v1/content/data/";

appimage::update::methods::PlingV1Zsync::PlingV1Zsync(std::vector<std::string> updateStringParts) :
        productId(updateStringParts[1]), fileMatchingPattern(updateStringParts[2]) {

}

std::vector<std::string> appimage::update::methods::PlingV1Zsync::getAvailableDownloads() {
    std::vector<std::string> downloads;

    cpr::Url productDetailsUrl = plingContentEndpointUrl + productId;
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
            if (fnmatch(fileMatchingPattern.data(), fileName.data(), 0) == 0)
                downloads.push_back(url);

            text = match.suffix();
        }
    }

    return downloads;
}

std::string appimage::update::methods::PlingV1Zsync::findLatestRelease(const std::vector<std::string>& downloads) {
    std::string latestReleaseUrl;
    std::string latestReleaseFileName;

    for (const auto& url: downloads) {
        auto file_name = url.substr(url.rfind('/') + 1);

        // keep only the latest release
        if (file_name > latestReleaseFileName) {
            latestReleaseUrl = std::string(url);
            latestReleaseFileName = std::string(file_name);
        }
    }

    return latestReleaseUrl;
}

std::string appimage::update::methods::PlingV1Zsync::resolveZsyncUrl(const std::string& downloadUrl) {
    // pling.com creates zsync files for every uploaded file, we just need to append .zsync
    return downloadUrl + ".zsync";
}

bool appimage::update::methods::PlingV1Zsync::isUpdateStringAccepted(std::vector<std::string> updateStringParts) {
    return updateStringParts.size() == 3 && updateStringParts[0] == "pling-v1-zsync";

}
