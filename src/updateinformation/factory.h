#pragma once

// local headers
#include "common.h"

namespace appimage::update::updateinformation {
    typedef std::shared_ptr<AbstractUpdateInformation> UpdateInformationPtr;

    UpdateInformationPtr  makeUpdateInformation(const std::string& rawUpdateInformation) {
        const auto updateInformationComponents = splitRawUpdateInformationComponents(rawUpdateInformation);

        if (updateInformationComponents.empty()) {
            throw UpdateInformationError("Update information invalid: | not found");
        }

        if (updateInformationComponents[0] == "zsync") {
            return std::make_shared<GenericZsyncUpdateInformation>(updateInformationComponents);
        } else if (updateInformationComponents[0] == "gh-releases-zsync") {
            // TODO: GitHub releases type should consider pre-releases when there's no other types of releases
            return std::make_shared<GithubReleasesUpdateInformation>(updateInformationComponents);
        } else if (updateInformationComponents[0] == "pling-v1-zsync") {
            return std::make_shared<PlingV1UpdateInformation>(updateInformationComponents);
        }

        throw UpdateInformationError("Unknown update information type: " + updateInformationComponents[0]);
    }
}
