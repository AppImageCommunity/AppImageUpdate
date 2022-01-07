// system headers
#include <deque>
#include <iostream>
#include <libgen.h>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

// library headers
#include <zsclient.h>
#include <hashlib/sha256.h>
#include <cpr/cpr.h>
#include <ftw.h>

// local headers
#include "appimage/update.h"
#include "updateinformation/updateinformation.h"
#include "appimage.h"
#include "util.h"

// convenience declaration
namespace {
    typedef std::lock_guard<std::mutex> lock_guard;
}

namespace appimage::update {
    using namespace util;

    class Updater::Private {
    public:
        explicit Private(const std::string& pathToAppImage) : state(INITIALIZED),
            appImage(pathToAppImage),
            zSyncClient(nullptr),
            thread(nullptr),
            mutex(),
            overwrite(false),
            rawUpdateInformation(appImage.readRawUpdateInformation())
        {};

    public:
        AppImage appImage;

        // we call this "raw" update information to highlight the difference between strings and the new
        // UpdateInformation infrastructure
        std::string rawUpdateInformation;

        // state
        State state;

        // ZSync client -- will be instantiated only if necessary
        std::shared_ptr<zsync2::ZSyncClient> zSyncClient;

        // threading
        std::thread* thread;
        std::mutex mutex;

        // status messages
        std::deque<std::string> statusMessages;

        // defines whether to overwrite original file
        bool overwrite;

    public:
        void issueStatusMessage(const std::string& message) {
            statusMessages.push_back(message);
        }

        StatusMessageCallback makeIssueStatusMessageCallback() {
            return [this](const std::string& message) {issueStatusMessage(message);};
        }

        void validateAppImage() {
            const auto rawUpdateInformationFromAppImage = appImage.readRawUpdateInformation();

            // first check whether there's update information at all
            if (rawUpdateInformationFromAppImage.empty()) {
                std::ostringstream oss;
                oss << "Could not find update information in the AppImage. "
                    << "Please contact the author of the AppImage and ask them to embed update information.";
                throw AppImageError(oss.str());
            }

            const auto updateInformationPtr = makeUpdateInformation(rawUpdateInformationFromAppImage);
            const auto zsyncUrl = updateInformationPtr->buildUrl(makeIssueStatusMessageCallback());

            // now check whether a ZSync URL could be composed by readAppImage
            // this is the only supported update type at the moment
            if (zsyncUrl.empty()) {
                std::ostringstream oss;
                oss << "ZSync URL not available. See previous messages for details.";
                throw AppImageError(oss.str());
            }
        }

        // thread runner
        void runUpdate() {
            // initialization
            try {
                lock_guard guard(mutex);

                // make sure it runs only once at a time
                // should never occur, but you never know
                if (state != INITIALIZED)
                    return;

                // if there is a ZSync client (e.g., because an update check has been run), clean it up
                // this ensures that a fresh instance will be used for the update run
                if (zSyncClient != nullptr) {
                    zSyncClient.reset();
                }

                validateAppImage();
                const auto updateInformationPtr = makeUpdateInformation(rawUpdateInformation);

                if (updateInformationPtr->type() == ZSYNC_GITHUB_RELEASES) {
                    issueStatusMessage("Updating from GitHub Releases via ZSync");
                } else if (updateInformationPtr->type() == ZSYNC_GENERIC) {
                    issueStatusMessage("Updating from generic server via ZSync");
                } else if (updateInformationPtr->type() == ZSYNC_PLING_V1) {
                    issueStatusMessage("Updating from Pling v1 server via ZSync");
                } else {
                    throw AppImageError("Unknown update information type");
                }

                const auto zsyncUrl = updateInformationPtr->buildUrl(makeIssueStatusMessageCallback());

                // doesn't matter which type it is exactly, they all work like the same
                zSyncClient = std::make_shared<zsync2::ZSyncClient>(zsyncUrl, appImage.path(), overwrite);

                // enable ranges optimizations
                zSyncClient->setRangesOptimizationThreshold(64 * 4096);

                // make sure the new AppImage goes into the same directory as the old one
                // unfortunately, to be able to use dirname(), one has to copy the C string first
                auto path = makeBuffer(appImage.path());
                std::string dirPath = dirname(path.data());

                zSyncClient->setCwd(dirPath);

                state = RUNNING;
            } catch (const AppImageError& e) {
                issueStatusMessage("Error reading AppImage: " + std::string(e.what()));
                state = ERROR;
                return;
            } catch (const UpdateInformationError& e) {
                issueStatusMessage("Failed to parse update information: " + std::string(e.what()));
                state = ERROR;
                return;
            }

            // keep state -- by default, an error (false) is assumed
            bool result = false;

            // run phase
            {
                // check whether it's a zsync operation
                if (zSyncClient != nullptr) {
                    result = zSyncClient->run();
                }
            }

            // end phase
            {
                lock_guard guard(mutex);

                if (result) {
                    state = SUCCESS;
                } else {
                    state = ERROR;
                }
            }
        }

        bool checkForChanges(bool& updateAvailable, const unsigned int method = 0) {
            lock_guard guard(mutex);

            if (state != INITIALIZED)
                return false;

            // validate AppImage
            try {
                validateAppImage();
            } catch (const AppImageError& e) {
                issueStatusMessage(e.what());
                return false;
            }

            try {
                auto updateInformationPtr = makeUpdateInformation(appImage.readRawUpdateInformation());
                const auto zsyncUrl = updateInformationPtr->buildUrl(makeIssueStatusMessageCallback());
                zSyncClient.reset(new zsync2::ZSyncClient(zsyncUrl, appImage.path()));
                return zSyncClient->checkForChanges(updateAvailable, method);
            } catch (const UpdateInformationError& e) {
                zSyncClient.reset();

                // return error in case of unknown update information
                issueStatusMessage(e.what());
                issueStatusMessage("Unknown update information type, aborting.");
                return false;
            }
        }
    };

    Updater::Updater(const std::string& pathToAppImage, bool overwrite) : d(new Updater::Private(ailfsRealpath(pathToAppImage))) {
        // workaround for AppImageLauncher filesystem
        d->overwrite = overwrite;

        // check whether file exists, otherwise throw exception
        std::ifstream f(d->appImage.path());

        if(!f || !f.good()) {
            auto errorMessage = std::strerror(errno);
            throw std::invalid_argument(errorMessage + std::string(": ") + d->appImage.path());
        }
    }

    Updater::~Updater() = default;

    void Updater::runUpdate() {
        // alias for private function
        return d->runUpdate();
    }

    bool Updater::start() {
        // lock mutex
        lock_guard guard(d->mutex);

        // prevent multiple start calls
        if(d->state != INITIALIZED)
            return false;

        // if there's a thread managed by this class already, should not start another one and lose access to
        // this one
        if(d->thread)
            return false;

        // create thread
        d->thread = new std::thread(&Updater::runUpdate, this);

        return true;
    }

    bool Updater::isDone() {
        lock_guard guard(d->mutex);

        return d->state != INITIALIZED && d->state != RUNNING && d->state != STOPPING;
    }

    bool Updater::hasError() {
        lock_guard guard(d->mutex);

        return d->state == ERROR;
    }

    bool Updater::progress(double& progress) {
        lock_guard guard(d->mutex);

        if (d->state == INITIALIZED) {
            // this protects update checks from returning progress, which would only occur when using method 0
            progress = 0;
            return true;
        } else if (d->state == SUCCESS || d->state == ERROR) {
            progress = 1;
            return true;
        }

        if (d->zSyncClient != nullptr) {
            progress = d->zSyncClient->progress();
            return true;
        }

        return false;
    }

    bool Updater::stop() {
        throw std::runtime_error("not implemented");
    }

    bool Updater::nextStatusMessage(std::string& message) {
        // first, check own message queue
        if (!d->statusMessages.empty()) {
            message = d->statusMessages.front();
            d->statusMessages.pop_front();
            return true;
        }

        // next, check zsync client for a message
        if (d->zSyncClient != nullptr) {
            std::string zsyncMessage;
            if (!d->zSyncClient->nextStatusMessage(zsyncMessage))
                return false;
            // show that the message is coming from zsync2
            message = "zsync2: " + zsyncMessage;
            return true;
        }

        return false;
    }

    Updater::State Updater::state() {
        return d->state;
    }

    bool Updater::checkForChanges(bool &updateAvailable, const unsigned int method) {
        return d->checkForChanges(updateAvailable, method);
    }

    bool Updater::describeAppImage(std::string& description) const {
        std::ostringstream oss;
        bool success = true;

        try {
            oss << "Parsing file: " << d->appImage.path() << std::endl;
            oss << "AppImage type: " << d->appImage.appImageType() << std::endl;

            const auto rawUpdateInformation = d->appImage.readRawUpdateInformation();

            oss << "Raw update information: ";
            if (rawUpdateInformation.empty())
                oss << "<empty>";
            else
                oss << rawUpdateInformation;
            oss << std::endl;

            auto updateInformation = makeUpdateInformation(rawUpdateInformation);

            oss << "Update information type: ";

            if (updateInformation->type() == ZSYNC_GENERIC)
                oss << "Generic ZSync URL";
            else if (updateInformation->type() == ZSYNC_GITHUB_RELEASES)
                oss << "ZSync via GitHub Releases";
            else if (updateInformation->type() == ZSYNC_PLING_V1)
                oss << "ZSync via OCS";
            else if (updateInformation->type() == INVALID)
                oss << "Invalid (parsing failed/no update information available)";
            else
                oss << "Unknown error";

            try {
                auto url = updateInformation->buildUrl(d->makeIssueStatusMessageCallback());

                oss << "Assembled ZSync URL: " << url << std::endl;

            } catch (const UpdateInformationError& e) {
                oss << "Failed to assemble ZSync URL. AppImageUpdate can not be used with this AppImage. "
                << "See below for more information"
                << std::endl
                << e.what();
            }
        } catch (const UpdateInformationError& e) {
            oss << e.what();
            success = false;
        }

        description = oss.str();

        return success;
    }

    bool Updater::pathToNewFile(std::string& path) const {
        // only available update method is via ZSync
        if (d->zSyncClient)
            return d->zSyncClient->pathToNewFile(path);

        return false;
    }

    bool Updater::remoteFileSize(off_t& fileSize) const {
        // only available update method is via ZSync
        if (d->zSyncClient != nullptr)
            return d->zSyncClient->remoteFileSize(fileSize);

        return false;
    }

    Updater::ValidationState Updater::validateSignature() {
        std::string pathToNewAppImage;
        if (!this->pathToNewFile(pathToNewAppImage)) {
            // return generic error
            return VALIDATION_FAILED;
        }

        AppImage newAppImage(pathToNewAppImage);

        auto pathToOldAppImage = abspath(d->appImage.path());
        if (pathToOldAppImage == pathToNewAppImage) {
            pathToOldAppImage = pathToNewAppImage + ".zs-old";
        }

        AppImage oldAppImage(pathToOldAppImage);

        auto oldSignature = oldAppImage.readSignature();
        auto newSignature = newAppImage.readSignature();

        // remove any spaces and/or newline characters there might be on the left or right of the payload
        zsync2::trim(oldSignature, '\n');
        zsync2::trim(newSignature, '\n');
        zsync2::trim(oldSignature);
        zsync2::trim(newSignature);

        auto oldSigned = !oldSignature.empty();
        auto newSigned = !newSignature.empty();

        auto oldDigest = oldAppImage.calculateHash();
        auto newDigest = newAppImage.calculateHash();

        auto oldDigestOrig = readElfSection(pathToOldAppImage, ".sha256_sig");
        auto newDigestOrig = readElfSection(pathToNewAppImage, ".sha256_sig");

        std::string tempDir;
        {
            auto buffer = makeBuffer("/tmp/AppImageUpdate-XXXXXX");

            if (mkdtemp(buffer.data()) == nullptr) {
                const int error = errno;
                const auto* errorMessage = strerror(error);
                d->issueStatusMessage("Failed to create temporary directory: " + std::string(errorMessage));
                return VALIDATION_TEMPDIR_CREATION_FAILED;
            }

            tempDir = buffer.data();
        }

        auto tempFile = [&tempDir](const std::string& filename, const std::string& contents) {
            std::stringstream path;
            path << tempDir << "/" << filename;

            auto x = path.str();

            std::ofstream ofs(path.str());
            ofs.write(contents.c_str(), contents.size());

            return path.str();
        };

        if (!oldSigned && !newSigned)
            return VALIDATION_NOT_SIGNED;

        else if (oldSigned && !newSigned)
            return VALIDATION_NO_LONGER_SIGNED;

        // store digests and signatures in files so they can be passed to gpg2
        auto oldDigestFilename = tempFile("old-digest", oldDigest);
        auto newDigestFilename = tempFile("new-digest", newDigest);

        auto oldSignatureFilename = tempFile("old-signature", oldSignature);
        auto newSignatureFilename = tempFile("new-signature", newSignature);

        auto cleanup = [&tempDir]() {
            // cleanup
            auto unlinkCb = [](const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf) {
                int rv = remove(fpath);
                if (rv)
                    perror(fpath);
                return rv;
            };
            nftw(tempDir.c_str(), unlinkCb, 64, FTW_DEPTH | FTW_PHYS);
            rmdir(tempDir.c_str());
        };

        // find gpg2 binary
        auto gpg2Path = findInPATH("gpg2");
        if (gpg2Path.empty()) {
            cleanup();
            return VALIDATION_GPG2_MISSING;
        }

        const auto tempKeyRingPath = tempDir + "/keyring";

        // create keyring file, otherwise GPG will complain
        std::ofstream ofs(tempKeyRingPath);
        ofs.close();

        auto importKeyFromAppImage = [&tempKeyRingPath, &gpg2Path](const std::string& path) {
            auto key = readElfSection(path, ".sig_key");

            if (key.empty())
                return false;

            std::ostringstream oss;
            oss << "'" << gpg2Path << "' "
                << "--no-default-keyring --keyring '" << tempKeyRingPath << "' --import";

            auto command = oss.str();

            auto proc = popen(oss.str().c_str(), "w");

            fwrite(key.c_str(), key.size(), sizeof(char), proc);

            auto retval = pclose(proc);

            return retval == 0;
        };

        importKeyFromAppImage(pathToOldAppImage);
        importKeyFromAppImage(pathToNewAppImage);

        auto verifySignature = [this, &gpg2Path, &tempKeyRingPath](
            const std::string& signatureFile, const std::string& digestFile,
            bool& keyFound, bool& goodSignature,
            std::string& keyID, std::string& keyOwner
        ) {
            std::ostringstream oss;
            oss << "'" << gpg2Path << "'"
                << " --keyring '" << tempKeyRingPath << "'"
                << " --verify '" << signatureFile << "' '" << digestFile << "' 2>&1";

            auto command = oss.str();

            // make sure output is in English
            setenv("LANGUAGE", "C", 1);
            setenv("LANG", "C", 1);
            setenv("LC_ALL", "C", 1);

            auto* proc = popen(command.c_str(), "r");

            if (proc == nullptr)
                return false;

            char* currentLine = nullptr;
            size_t lineSize = 0;

            keyFound = true;
            goodSignature = false;

            while (getline(&currentLine, &lineSize, proc) != -1) {
                std::string line = currentLine;

                trim(line, '\n');
                trim(line);

                d->issueStatusMessage(std::string("gpg2: ") + line);

                auto splitOwner = [&line]() {
                    auto parts = split(line, '"');

                    if (parts.size() < 2)
                        return std::string();

                    auto skip = parts[0].length();

                    return line.substr(skip + 1, line.length() - skip - 2);
                };

                if (stringStartsWith(line, "gpg: Signature made")) {
                    // extract key
                    auto parts = split(line);

                    if (*(parts.end() - 3) == "key" && *(parts.end() - 2) == "ID")
                        keyID = parts.back();

                } else if (stringStartsWith(line, "gpg: Good signature from")) {
                    goodSignature = true;
                    keyOwner = splitOwner();
                } else if (stringStartsWith(line, "gpg: BAD signature from")) {
                    goodSignature = false;
                    keyOwner = splitOwner();
                } else if (stringStartsWith(line, "gpg: Can't check signature: No public key")) {
                    keyFound = false;
                }
            }

            auto rv = pclose(proc);

            return true;
        };

        bool oldKeyFound = false, newKeyFound = false, oldSignatureGood = false, newSignatureGood = false;
        std::string oldKeyID, newKeyID, oldKeyOwner, newKeyOwner;

        if (oldSigned) {
            if (!verifySignature(oldSignatureFilename, oldDigestFilename, oldKeyFound, oldSignatureGood, oldKeyID, oldKeyOwner)) {
                cleanup();
                return VALIDATION_GPG2_CALL_FAILED;
            }
        }

        if (newSigned) {
            if (!verifySignature(newSignatureFilename, newDigestFilename, newKeyFound, newSignatureGood, newKeyID, newKeyOwner)) {
                cleanup();
                return VALIDATION_GPG2_CALL_FAILED;
            }
        }

        if (!oldKeyFound || !newKeyFound) {
            // if the keys haven't been embedded in the AppImages, we treat them as not signed
            // see https://github.com/AppImage/AppImageUpdate/issues/16#issuecomment-370932698 for details
            cleanup();
            return VALIDATION_NOT_SIGNED;
        }

        if (!oldSignatureGood || !newSignatureGood) {
            cleanup();
            return VALIDATION_BAD_SIGNATURE;
        }

        if (oldSigned && newSigned) {
            if (oldKeyID != newKeyID) {
                cleanup();
                return VALIDATION_KEY_CHANGED;
            }
        }

        cleanup();
        return VALIDATION_PASSED;
    }

    std::string Updater::signatureValidationMessage(const Updater::ValidationState& state) {
        static const std::map<ValidationState, std::string> validationMessages = {
            {VALIDATION_PASSED, "Signature validation successful"},

            // warning states
            {VALIDATION_WARNING, "Signature validation warning"},
            {VALIDATION_NOT_SIGNED, "AppImage not signed"},
            {VALIDATION_GPG2_MISSING, "gpg2 command not found, please install"},

            // error states
            {VALIDATION_FAILED, "Signature validation failed"},
            {VALIDATION_GPG2_CALL_FAILED, "Call to gpg2 failed"},
            {VALIDATION_TEMPDIR_CREATION_FAILED, "Failed to create temporary directory"},
            {VALIDATION_NO_LONGER_SIGNED, "AppImage no longer comes with signature"},
            {VALIDATION_BAD_SIGNATURE, "Bad signature"},
            {VALIDATION_KEY_CHANGED, "Key changed for signing AppImages"},
        };

        if (validationMessages.count(state) > 0) {
            return validationMessages.at(state);
        }

        return "Unknown validation state";
    }

    void Updater::restoreOriginalFile() {
        std::string newFilePath;

        if (!pathToNewFile(newFilePath)) {
            throw std::runtime_error("Failed to get path to new file");
        }

        // make sure to compare absolute, resolved paths
        newFilePath = abspath(newFilePath);

        const auto& oldFilePath = abspath(d->appImage.path());

        // restore original file
        std::remove(newFilePath.c_str());

        if (oldFilePath == newFilePath) {
            std::rename((newFilePath + ".zs-old").c_str(), newFilePath.c_str());
        }
    }

    void Updater::copyPermissionsToNewFile() {
        std::string oldFilePath = abspath(d->appImage.path());

        std::string newFilePath;

        if (!pathToNewFile(newFilePath)) {
            throw std::runtime_error("Failed to get path to new file");
        }

        // make sure to compare absolute, resolved paths
        newFilePath = abspath(newFilePath);

        appimage::update::copyPermissions(oldFilePath, newFilePath);
    }

    std::string Updater::updateInformation() const {
        return d->rawUpdateInformation;
    }

    void Updater::setUpdateInformation(std::string newUpdateInformation) {
        d->rawUpdateInformation = std::move(newUpdateInformation);
    }
}
