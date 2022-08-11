// system headers
#include <cassert>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

// library headers
#include <gpgme.h>

// local headers
#include "signaturevalidator.h"
#include "util/util.h"

namespace appimage::update::signing {
    using namespace util;

    class GpgmeInMemoryData {
    private:
        gpgme_data_t _dh = nullptr;

    public:
        explicit GpgmeInMemoryData(const std::string& buffer) {
            const auto error = gpgme_data_new_from_mem(&_dh, buffer.c_str(), buffer.size(), true);
            if (error != GPG_ERR_NO_ERROR) {
                throw GpgError(error, "failed to initialize in-memory data for gpgme");
            }
        }

        ~GpgmeInMemoryData() noexcept {
            gpgme_data_release(_dh);
        }

        [[nodiscard]] auto get() const {
            return _dh;
        }
    };

    class GpgmeContext {
    private:
        gpgme_ctx_t _ctx = nullptr;

        static void gpgmeThrowIfNecessary(gpg_err_code_t error, const std::string& message) {
            if (error != GPG_ERR_NO_ERROR) {
                throw GpgError(error, message);
            }
        }

        static void gpgmeThrowIfNecessary(gpgme_error_t error, const std::string& message) {
            return gpgmeThrowIfNecessary(gpgme_err_code(error), message);
        }

    public:
        explicit GpgmeContext(const std::string& gnupgHome) {
            static const char gpgme_minimum_required_version[] = "1.10.0";
            const char* gpgme_version = gpgme_check_version(gpgme_minimum_required_version);

            if (gpgme_version == nullptr) {
                std::stringstream error;
                error << "could not initialize gpgme (>= " << gpgme_minimum_required_version << ")";
                throw GpgError(GPG_ERR_NO_ERROR, error.str());
            }

            gpgmeThrowIfNecessary(gpgme_new(&_ctx), "failed to initialize gpgme context");
            assert(_ctx != nullptr);

            gpgmeThrowIfNecessary(gpgme_set_ctx_flag(_ctx, "full-status", "1"), "failed to initialize gpgme context");
            gpgmeThrowIfNecessary(gpgme_set_protocol(_ctx, GPGME_PROTOCOL_OpenPGP), "failed to set OpenPGP protocol");

            auto engine_info = gpgme_ctx_get_engine_info(_ctx);

            while (engine_info && engine_info->protocol != gpgme_get_protocol(_ctx)) {
                engine_info = engine_info->next;
            }

            // experience within AppImageKit shows that gnupg versions <= 2.2 are likely to cause issues
            // therefore, we warn users if an incompatible version was found
            {
                const std::string format = "%lu.%lu";
                unsigned long majorVersion, minorVersion;

                if (sscanf(engine_info->version, format.c_str(), &majorVersion, &minorVersion) < 2) {
                    throw GpgError(GPG_ERR_NO_ERROR, "failed to parse engine version number");
                }

                if (majorVersion != 2 || minorVersion < 2) {
                    // TODO: use regular logging system
                    std::cerr << "gpg engine version " << engine_info->version << " is likely incompatible, "
                              << "consider using version >= 2.2" << std::endl;
                }
            }

            if (!gnupgHome.empty()) {
                // we reuse the existing engine configuration, but use a custom home dir
                gpgmeThrowIfNecessary(
                    gpgme_ctx_set_engine_info(_ctx, engine_info->protocol, engine_info->file_name, gnupgHome.c_str()),
                    "failed to set engine info"
                );
            }
        }

        void importKey(const std::string& key) {
            GpgmeInMemoryData data(key);

            gpgmeThrowIfNecessary(gpgme_op_import(_ctx, data.get()), "failed to import key");

            auto result = gpgme_op_import_result(_ctx);

            // some "assertions" to make sure importing worked
            if (result->not_imported > 0) {
                std::stringstream errorMessage;
                errorMessage << result->not_imported << " keys could not be imported";
                throw GpgError(GPG_ERR_NO_ERROR, errorMessage.str());
            }
            if (result->imported < 0) {
                throw GpgError(GPG_ERR_NO_ERROR, "result implies no keys were imported");
            }
        }

        ~GpgmeContext() {
            gpgme_release(_ctx);
        }

        SignatureValidationResult validateSignature(const std::string& signedDataString, const std::string& signatureString) {
            GpgmeInMemoryData signedDataData(signedDataString);
            GpgmeInMemoryData signatureData(signatureString);

            const auto error = gpgme_err_code(gpgme_op_verify(_ctx, signatureData.get(), signedDataData.get(), nullptr));

            std::stringstream errorMessage;

            switch (error) {
                case GPG_ERR_NO_ERROR: {
                    break;
                }
                case GPG_ERR_INV_VALUE: {
                    // this should never ever happen, and implies an issue within our code
                    throw GpgError(error, "unexpected error while validating signature");
                }
                default: {
                    errorMessage << "unexpected error";
                    return {SignatureValidationResult::ResultType::ERROR, errorMessage.str(), {}};
                }
            }

            auto verificationResult = gpgme_op_verify_result(_ctx);

            auto signature = verificationResult->signatures;

            if (signature == nullptr) {
                return {SignatureValidationResult::ResultType::ERROR, "no signatures found", {}};
            }

            std::stringstream message;
            std::vector<std::string> fingerprints;
            // we're optimistic: we assume the result is good unless we find clues it's not
            SignatureValidationResult::ResultType resultType = SignatureValidationResult::ResultType::SUCCESS;

            // there should not be more than one signature, but we don't know for sure
            do {
                fingerprints.emplace_back(signature->fpr);

                message << "Signature checked for key with fingerprint " << signature->fpr << ": ";
                if (
                    (signature->summary & GPGME_SIGSUM_VALID | signature->summary & GPGME_SIGSUM_GREEN) != 0 ||

                    // according to rpm, signature is valid but the key is not certified with a trusted signature
                    (signature->summary == 0 && signature->status == GPG_ERR_NO_ERROR)
                ) {
                    // valid signature. no change to status required
                } else if (
                    // an expired signature or key may happen any time with AppImages
                    // as long as the signature itself is valid, we report a warning state
                    (signature->summary & GPGME_SIGSUM_KEY_EXPIRED | signature->summary & GPGME_SIGSUM_KEY_MISSING) > 0
                ) {
                    message << "warning";
                    if (resultType < SignatureValidationResult::ResultType::WARNING) {
                        resultType = SignatureValidationResult::ResultType::WARNING;
                    }
                } else {
                    message << "error";
                    // invalid signature
                    resultType = SignatureValidationResult::ResultType::ERROR;
                }

                std::vector<std::string> summaryInfos;

                // inform user about other information we can gather from the summary
                if ((signature->summary & GPGME_SIGSUM_KEY_REVOKED) > 0) {
                    summaryInfos.emplace_back("key revoked");
                }
                if ((signature->summary & GPGME_SIGSUM_KEY_EXPIRED) > 0) {
                    summaryInfos.emplace_back("key expired");
                }
                if ((signature->summary & GPGME_SIGSUM_SIG_EXPIRED) > 0) {
                    summaryInfos.emplace_back("signature expired");
                }
                if ((signature->summary & GPGME_SIGSUM_KEY_MISSING) > 0) {
                    summaryInfos.emplace_back("key missing");
                }
                if ((signature->summary & GPGME_SIGSUM_CRL_MISSING) > 0) {
                    summaryInfos.emplace_back("CRL missing");
                }
                if ((signature->summary & GPGME_SIGSUM_CRL_TOO_OLD) > 0) {
                    summaryInfos.emplace_back("CRL too old");
                }
                if ((signature->summary & GPGME_SIGSUM_BAD_POLICY) > 0) {
                    summaryInfos.emplace_back("bad polcy");
                }
                if ((signature->summary & GPGME_SIGSUM_SYS_ERROR) > 0) {
                    summaryInfos.emplace_back("system error");
                }
                if ((signature->summary & GPGME_SIGSUM_TOFU_CONFLICT) > 0) {
                    summaryInfos.emplace_back("TOFU conflict");
                }

                message << join(summaryInfos, ", ") << std::endl;
            } while (signature->next != nullptr);

            switch (resultType) {
                case SignatureValidationResult::ResultType::SUCCESS: {
                    message << "Validation successful";
                    break;
                }
                case SignatureValidationResult::ResultType::WARNING: {
                    message << "Validation resulted in warning state";
                    break;
                }
                case SignatureValidationResult::ResultType::ERROR: {
                    message << "Validation failed";
                    break;
                }
            }

            return {resultType, message.str(), fingerprints};
        }
    };

    class GpgError::Private {
    public:
        std::string what;

        Private(gpg_error_t error, const std::string& message)
        {
            std::ostringstream oss;
            oss << message;

            if (error != GPG_ERR_NO_ERROR) {
                oss << " (gpg error: " << gpgme_strerror(error) << ")";
            }

            what = oss.str();
        }
    };

    GpgError::GpgError(gpg_error_t error, const std::string& message) :
        d(new Private(error, message))
    {}

    GpgError::~GpgError() noexcept = default;

    const char* GpgError::what() const noexcept {
        return d->what.c_str();
    }


    class SignatureValidationResult::Private {
    public:
        Private(ResultType type, const std::string& description, const std::vector<std::string>& keyFingerprints) :
            type(type),
            description(description),
            keyFingerprints(keyFingerprints)
        {}

        ResultType type;
        std::string description;
        std::vector<std::string> keyFingerprints;
    };

    SignatureValidationResult::SignatureValidationResult(ResultType type, const std::string& description, const std::vector<std::string>& keyFingerprints) :
        d(new Private(type, description, keyFingerprints))
    {}

    SignatureValidationResult::ResultType SignatureValidationResult::type() const {
        return d->type;
    }

    std::string SignatureValidationResult::message() const {
        return d->description;
    }

    std::vector<std::string> SignatureValidationResult::keyFingerprints() const {
        return d->keyFingerprints;
    }

    SignatureValidationResult::~SignatureValidationResult() = default;


    class SignatureValidator::Private {
    public:
        // we want to initialize this only once, since the constructor may have side effects on the system
        std::unique_ptr<GpgmeContext> context = nullptr;

        // we need a temporary keyring to work with
        std::filesystem::path tempGpgHomeDir;

        explicit Private() {
            std::string tempGpgHomeDirTemplate = std::filesystem::temp_directory_path() / "appimageupdate-XXXXXX";
            std::vector<char> tempGpgHomeDirCStr(tempGpgHomeDirTemplate.begin(), tempGpgHomeDirTemplate.end());

            if (mkdtemp(tempGpgHomeDirCStr.data()) == nullptr) {
                const auto error = errno;
                throw std::runtime_error(
                    std::string("failed to create temporary directory: ") + strerror(error)
                );
            }

            tempGpgHomeDir = std::string(tempGpgHomeDirCStr.data());

            {
                // create keyring file, otherwise GPG will likely complain
                std::ofstream ofs(tempGpgHomeDir / "keyring");
            }

            context = std::make_unique<GpgmeContext>(tempGpgHomeDir);
        }

        ~Private() noexcept {
            // clean up temporary home
            std::filesystem::remove_all(tempGpgHomeDir);
        }
    };

    SignatureValidator::SignatureValidator() : d(new Private) {}

    SignatureValidationResult SignatureValidator::validate(const UpdatableAppImage& appImage) {
        d->context->importKey(appImage.readSigningKey());

        auto hashData = appImage.calculateHash();
        auto signatureData = appImage.readSignature();
        return d->context->validateSignature(hashData, signatureData);
    }

    SignatureValidator::~SignatureValidator() = default;
}
