#include "GenericZsyncUpdateInformation.h"

namespace appimage::update::updateinformation {

    GenericZsyncUpdateInformation::GenericZsyncUpdateInformation(
        const std::vector<std::string>& updateInformationComponents) :
        AbstractUpdateInformation(updateInformationComponents, ZSYNC_GENERIC)
    {
        // validation
        assertParameterCount(_updateInformationComponents, 2);
    }

    std::string GenericZsyncUpdateInformation::buildUrl(const StatusMessageCallback& issueStatusMessage) const {
        (void) issueStatusMessage;

        return _updateInformationComponents.back();
    }
}
