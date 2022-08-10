#pragma once

// local headers
#include "common.h"
#include "AbstractUpdateInformation.h"

namespace appimage::update::updateinformation {
    /**
     * Pling is a family of services which include an AppImage store among many other things. AppImage files are
     * served from the www.appimagehub.com and www.pling.com urls. They are also available through an xml API
     * `https://api.pling.com/ocs/v1/`
     *
     * format: pling-v1-zsync|<content id>|<file name matching pattern>
     */
    class PlingV1UpdateInformation : public AbstractUpdateInformation {
    private:
        std::string _fileMatchingPattern;
        std::string _productId;

    public:
        explicit PlingV1UpdateInformation(const std::vector<std::string>& updateInformationComponents);

    private:
        [[nodiscard]] std::vector<std::string> _getAvailableDownloads() const;

        static std::string _findLatestRelease(const std::vector<std::string>& downloads);

        static std::string _resolveZsyncUrl(const std::string& downloadUrl);

        [[nodiscard]] std::string buildUrl(const StatusMessageCallback& issueStatusMessage) const override;
    };
}
