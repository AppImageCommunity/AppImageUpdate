#pragma once

#include "AbstractUpdateInformation.h"

// system headers
#include <memory>

namespace appimage::update::updateinformation {
    typedef std::shared_ptr<AbstractUpdateInformation> UpdateInformationPtr;

    UpdateInformationPtr makeUpdateInformation(const std::string& rawUpdateInformation);
}
