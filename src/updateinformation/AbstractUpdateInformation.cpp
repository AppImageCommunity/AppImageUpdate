#include "AbstractUpdateInformation.h"

namespace appimage::update::updateinformation {
    AbstractUpdateInformation::AbstractUpdateInformation(std::vector<std::string> updateInformationComponents,
                                                         UpdateInformationType type) :
        _updateInformationComponents(std::move(updateInformationComponents)),
        _type(type)
    {
    }

    void AbstractUpdateInformation::assertParameterCount(const std::vector<std::string>& uiComponents, size_t expectedSize) {
        if (uiComponents.size() != expectedSize) {
            std::ostringstream oss;
            oss << "Update information has invalid parameter count. Please contact the author of "
                << "the AppImage and ask them to revise the update information. They should consult "
                << "the AppImage specification, there might have been changes to the update"
                <<  "information.";

            throw UpdateInformationError(oss.str());
        }
    }

    UpdateInformationType AbstractUpdateInformation::type() const {
        return _type;
    }
}
