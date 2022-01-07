#pragma once

// local headers
#include <utility>

#include "common.h"

namespace appimage::update::updateinformation {
    class AbstractUpdateInformation {
    protected:
        const std::vector<std::string> _updateInformationComponents;
        const UpdateInformationType _type;

    protected:
        explicit AbstractUpdateInformation(std::vector<std::string> updateInformationComponents, UpdateInformationType type) :
            _updateInformationComponents(std::move(updateInformationComponents)),
            _type(type)
        {
        }

    protected:
        // another little helper
        static inline void assertParameterCount(const std::vector<std::string>& uiComponents, size_t expectedSize) {
            if (uiComponents.size() != expectedSize) {
                std::ostringstream oss;
                oss << "Update information has invalid parameter count. Please contact the author of "
                << "the AppImage and ask them to revise the update information. They should consult "
                << "the AppImage specification, there might have been changes to the update"
                <<  "information.";

                throw UpdateInformationError(oss.str());
            }
        }

    public:
        [[nodiscard]] UpdateInformationType type() const {
            return _type;
        }

        [[nodiscard]] virtual std::string buildUrl(const StatusMessageCallback& issueStatusMessage) const = 0;
    };
}
