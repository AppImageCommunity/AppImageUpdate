#include "GithubReleasesZsyncUpdateInformation.h"

/* Eh, borrowed from zsync2. Why reinvent the wheel? */
#include <sys/stat.h>

// returns non-zero ("true") on success, 0 ("false") on failure
int file_exists(const char* path) {
    struct stat statbuf = {};

    if (stat(path, &statbuf) == 0) {
        return 1;
    }

    const int error = errno;
    if (error != ENOENT) {
        fprintf(stderr, "zsync2: Unknown error while checking whether file %s exists: %s\n", path, strerror(error));
    }

    return 0;
}

// memory returned by this function must not be freed by the user
const char* ca_bundle_path() {
    // in case the user specifies a custom file, we use this one instead
    // TODO: consider merging the user-specified file with the system CA bundle into a temp file
    const char* path_from_environment = getenv("SSL_CERT_FILE");

    if (path_from_environment != NULL) {
        return path_from_environment;
    }

#if CURL_AT_LEAST_VERSION(7, 84, 0)
    {
        // (very) recent libcurl versions provide us with a way to query the build-time search path they
        // use to find a suitable (distro-provided) CA certificate bundle they can hardcode
        // we can use this value to search for a bundle dynamically in the application
        CURL* curl = curl_easy_init();

        if (curl != NULL) {
            CURLcode res;
            char* curl_provided_path = NULL;

            curl_easy_getinfo(curl, CURLINFO_CAINFO, &curl_provided_path);

            // should be safe to delete the handle and use the returned value, since it is allocated statically within libcurl
            curl_easy_cleanup(curl);

            if (curl_provided_path != NULL) {
                if (file_exists(curl_provided_path)) {
                    return curl_provided_path;
                }
            }
        }
    }
#endif

    // this list is a compilation of other AppImage projects' lists and the one used in libcurl's build system's autodiscovery
    // should cover most Linux distributions
    static const char* const possible_ca_bundle_paths[] = {
        "/etc/pki/tls/cacert.pem",
        "/etc/pki/tls/cert.pem",
        "/etc/pki/tls/certs/ca-bundle.crt",
        "/etc/ssl/ca-bundle.pem",
        "/etc/ssl/cert.pem",
        "/etc/ssl/certs/ca-certificates.crt",
        "/usr/local/share/certs/ca-root-nss.crt",
        "/usr/share/ssl/certs/ca-bundle.crt",
    };

    for (size_t i = 0; i < sizeof(possible_ca_bundle_paths); ++i) {
        const char* path_to_check = possible_ca_bundle_paths[i];
        if (file_exists(path_to_check)) {
            return path_to_check;
        }
    }

    return NULL;
}
/* End of zsync2 code */

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

        cpr::Session session;
        cpr::SslOptions options;
        options.ca_info = std::string(ca_bundle_path());
        session.SetSslOptions(options);
        session.SetUrl(cpr::Url{url.str()});
        auto response = session.Get();

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
            const auto name = asset["name"].get<std::string>();

            if (fnmatch(pattern.c_str(), name.c_str(), 0) == 0) {
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
