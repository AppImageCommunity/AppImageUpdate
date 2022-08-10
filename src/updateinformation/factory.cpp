#include "factory.h"
#include "GenericZsyncUpdateInformation.h"
#include "GithubReleasesZsyncUpdateInformation.h"
#include "PlingV1UpdateInformation.h"

namespace appimage::update::updateinformation {
    std::shared_ptr<AbstractUpdateInformation> makeUpdateInformation(const std::string& rawUpdateInformation) {
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
