#pragma once

// system headers
#include <fstream>
#include <utility>

namespace appimage::update {
    class AppImageError : public std::runtime_error {
        using std::runtime_error::runtime_error;
    };

    class UpdatableAppImage {
    private:
        std::string _path;

    private:
        void assertIfstreamGood(const std::ifstream& ifs) const;

        [[nodiscard]] std::ifstream _open() const;

        bool _hasElfMagicValue(std::ifstream& ifs) const;

        bool _hasIsoMagicValue(std::ifstream& ifs) const;

    public:
        explicit UpdatableAppImage(std::string path);

        [[nodiscard]] std::string path() const;

        [[nodiscard]] int appImageType() const;

        [[nodiscard]] std::string readSignature() const;

        [[nodiscard]] std::string readSigningKey() const;

        [[nodiscard]] std::string readRawUpdateInformation() const;

        [[nodiscard]] std::string calculateHash() const;
    };
}
