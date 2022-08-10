#pragma once

// system headers
#include <sstream>

// local headers
#include <utility>

#include "common.h"

namespace appimage::update::updateinformation {
    class AbstractUpdateInformation {
    protected:
        const std::vector<std::string> _updateInformationComponents;
        const UpdateInformationType _type;

    protected:
        explicit AbstractUpdateInformation(std::vector<std::string> updateInformationComponents, UpdateInformationType type);

    protected:
        // another little helper
        static void assertParameterCount(const std::vector<std::string>& uiComponents, size_t expectedSize);

    public:
        [[nodiscard]] UpdateInformationType type() const;

        [[nodiscard]] virtual std::string buildUrl(const StatusMessageCallback& issueStatusMessage) const = 0;
    };
}
