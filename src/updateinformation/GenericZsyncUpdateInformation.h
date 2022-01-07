#pragma once

// local headers
#include "common.h"

namespace appimage::update::updateinformation {
    class GenericZsyncUpdateInformation : public AbstractUpdateInformation {
    public:
        explicit GenericZsyncUpdateInformation(const std::vector<std::string>& updateInformationComponents) :
            AbstractUpdateInformation(updateInformationComponents, ZSYNC_GENERIC)
        {
            // validation
            assertParameterCount(_updateInformationComponents, 2);
        }

    public:
        [[nodiscard]] std::string buildUrl(const StatusMessageCallback& issueStatusMessage) const override {
            (void) issueStatusMessage;

            return _updateInformationComponents.back();
        }
    };
}
