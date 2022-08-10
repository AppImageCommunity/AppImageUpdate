#pragma once

// local headers
#include "common.h"
#include "AbstractUpdateInformation.h"

namespace appimage::update::updateinformation {
    class GenericZsyncUpdateInformation : public AbstractUpdateInformation {
    public:
        explicit GenericZsyncUpdateInformation(const std::vector<std::string>& updateInformationComponents);

    public:
        [[nodiscard]] std::string buildUrl(const StatusMessageCallback& issueStatusMessage) const override;
    };
}
