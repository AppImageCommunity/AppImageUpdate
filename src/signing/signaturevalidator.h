#pragma once

// system headers
#include <memory>
#include <filesystem>

// library headers
#include <gpg-error.h>

// local headers
#include "util/updatableappimage.h"

namespace appimage::update::signing {
    class GpgError : public std::exception {
    public:
        explicit GpgError(gpg_error_t error, const std::string& message);
        ~GpgError() noexcept;

        const char* what() const noexcept override;

    private:
        class Private;
        std::unique_ptr<Private> d;
    };

    class SignatureValidationResult {
    public:
        enum class ResultType {
            SUCCESS,
            WARNING,
            ERROR,
        };

        SignatureValidationResult(ResultType type, const std::string& description, const std::vector<std::string>& keyFingerprints = {});

        // required to make PImpl work with unique_ptr
        ~SignatureValidationResult() noexcept;

        ResultType type() const;
        std::string message() const;
        std::vector<std::string> keyFingerprints() const;

    private:
        class Private;
        std::unique_ptr<Private> d;
    };

    class SignatureValidator {
    public:
        explicit SignatureValidator();

        // required to make PImpl work with unique_ptr
        ~SignatureValidator() noexcept;

        SignatureValidationResult validate(const UpdatableAppImage& appImage);

    private:
        class Private;
        std::unique_ptr<Private> d;
    };
}

