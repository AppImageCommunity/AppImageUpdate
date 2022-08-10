#pragma once

// system headers
#include <fnmatch.h>

// library headers
#include <nlohmann/json.hpp>
#include <cpr/cpr.h>

// local headers
#include "common.h"
#include "AbstractUpdateInformation.h"

namespace appimage::update::updateinformation {
    class GithubReleasesUpdateInformation : public AbstractUpdateInformation {
    public:
        explicit GithubReleasesUpdateInformation(const std::vector<std::string>& updateInformationComponents);

    public:
        [[nodiscard]] std::string buildUrl(const StatusMessageCallback& issueStatusMessage) const override;
    };
}
