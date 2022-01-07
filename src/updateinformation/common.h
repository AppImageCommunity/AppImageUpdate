#pragma once

// system headers
#include <functional>
#include <stdexcept>

// local headers
#include "util.h"

namespace appimage::update::updateinformation {
    class UpdateInformationError : public std::runtime_error {
        using std::runtime_error::runtime_error;
    };

    enum UpdateInformationType {
        INVALID = -1,
        ZSYNC_GENERIC = 0,
        ZSYNC_GITHUB_RELEASES = 1,
        // ZSYNC_BINTRAY is deprecated
        ZSYNC_PLING_V1 = 3,
    };

    using StatusMessageCallback = std::function<void(const std::string&)>;

    // little helper
    inline std::vector<std::string> splitRawUpdateInformationComponents(const std::string& rawUpdateInformation) {
        return appimage::update::util::split(rawUpdateInformation, '|');
    }
}
