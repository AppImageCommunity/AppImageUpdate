// system headers+
#include <algorithm>
#include <chrono>
#include <deque>
#include <fnmatch.h>
#include <fstream>
#include <iostream>
#include <libgen.h>
#include <mutex>
#include <sstream>
#include <thread>
#include <unistd.h>

// library headers
#include <zsclient.h>
#include <hashlib/sha256.h>
#include <cpr/cpr.h>
#include <ftw.h>

// local headers
#include "appimage/update.h"
#include "util.h"

// convenience declaration
typedef std::lock_guard<std::mutex> lock_guard;

namespace appimage {
    namespace update {
        class Updater::Private {
        public:
            Private() : state(INITIALIZED),
                pathToAppImage(),
                zSyncClient(nullptr),
                thread(nullptr),
                mutex(),
                overwrite(false)
            {};

            ~Private() {
                delete zSyncClient;
            }

        public:
            // data
            std::string pathToAppImage;

            // state
            State state;

            // ZSync client -- will be instantiated only if necessary
            zsync2::ZSyncClient* zSyncClient;

            // threading
            std::thread* thread;
            std::mutex mutex;

            // status messages
            std::deque<std::string> statusMessages;

            // defines whether to overwrite original file
            bool overwrite;

        public:
            enum UpdateInformationType {
                INVALID = -1,
                ZSYNC_GENERIC = 0,
                ZSYNC_GITHUB_RELEASES,
                ZSYNC_BINTRAY,
            };

            struct AppImage {
                std::string filename;
                int appImageVersion;
                std::string rawUpdateInformation;
                UpdateInformationType updateInformationType;
                std::string zsyncUrl;
                std::string signature;

                AppImage() : appImageVersion(-1), updateInformationType(INVALID) {};
            };
            typedef struct AppImage AppImage;

        public:
            void issueStatusMessage(const std::string& message) {
                statusMessages.push_back(message);
            }

            static const std::string readAppImageSignature(const std::string& pathToAppImage) {
                return readElfSection(pathToAppImage, ".sha256_sig");
            }

            static const std::string hashAppImage(const std::string& pathToAppImage) {
                // read offset and length of signature section to skip it later
                unsigned long sigOffset = 0, sigLength = 0;
                unsigned long keyOffset = 0, keyLength = 0;

                if (!appimage_get_elf_section_offset_and_length(pathToAppImage.c_str(), ".sha256_sig", &sigOffset, &sigLength))
                    return "";

                if (!appimage_get_elf_section_offset_and_length(pathToAppImage.c_str(), ".sig_key", &keyOffset, &keyLength))
                    return "";

                std::ifstream ifs(pathToAppImage);

                if (!ifs)
                    return "";

                SHA256 digest;

                // validate.c uses "offset" as chunk size, but that value might be quite high, and therefore uses
                // a lot of memory
                // TODO: use a smaller value (maybe use a prime factorization and use the biggest prime factor?)
                const ssize_t chunkSize = 4096;

                std::vector<char> buffer(chunkSize, 0);

                ssize_t totalBytesRead = 0;

                // bytes that should be skipped when reading the next chunk
                // when e.g., a section that must be ignored spans over more than one chunk, this amount of bytes is
                // being nulled & skipped before reading data from the file again
                std::streamsize bytesToSkip = 0;

                ifs.seekg (0, ifs.end);
                const ssize_t fileSize = ifs.tellg();
                ifs.seekg (0, ifs.beg);

                while (ifs) {
                    ssize_t bytesRead = 0;

                    auto bytesLeftInChunk = std::min(chunkSize, (fileSize - totalBytesRead));

                    if (bytesLeftInChunk <= 0)
                        break;

                    auto skipBytes = [&bytesRead, &bytesLeftInChunk, &buffer, &ifs, &totalBytesRead](ssize_t count) {
                        if (count <= 0)
                            return;

                        std::fill(buffer.begin(), buffer.begin() + count, '\0');
                        bytesRead += count;
                        totalBytesRead += count;
                        bytesLeftInChunk -= count;
                        ifs.seekg(count, std::ios::cur);
                    };

                    auto readBytes = [&bytesRead, &bytesLeftInChunk, &buffer, &ifs, &totalBytesRead](ssize_t count) {
                        if (count <= 0)
                            return;

                        ifs.read(buffer.data(), count);
                        bytesRead += ifs.gcount();
                        totalBytesRead += count;
                        bytesLeftInChunk -= bytesRead;
                    };

                    auto checkSkipSection = [&](const ssize_t sectionOffset, const ssize_t sectionLength) {
                        // check whether signature starts in current chunk
                        const ssize_t sectionOffsetDelta = sectionOffset - totalBytesRead;

                        if (sectionOffsetDelta >= 0 && sectionOffsetDelta < bytesLeftInChunk) {
                            // read until section begins
                            readBytes(sectionOffsetDelta);

                            // calculate how many bytes must be nulled in this chunk
                            // the rest will be nulled in the following chunk(s)
                            auto bytesLeft = sectionLength;
                            const auto bytesToNullInCurrentChunk = std::min(bytesLeftInChunk, bytesLeft);

                            // null these bytes
                            skipBytes(bytesToNullInCurrentChunk);

                            // calculate how many bytes must be nulled in future chunks
                            bytesLeft -= bytesToNullInCurrentChunk;
                            bytesToSkip = bytesLeft;
                        }
                    };

                    // check whether bytes must be skipped from previous sections
                    if (bytesToSkip > 0) {
                        auto bytesToSkipInCurrentChunk = std::min(chunkSize, bytesToSkip);
                        skipBytes(bytesToSkipInCurrentChunk);
                        bytesToSkip -= bytesToSkipInCurrentChunk;
                    }

                    // check whether one of the sections that must be skipped are in the current chunk, and if they
                    // are, skip those sections in the current and future sections
                    checkSkipSection(sigOffset, sigLength);
                    checkSkipSection(keyOffset, keyLength);

                    // read remaining bytes in chunk, given the file has still data to be read
                    if (ifs && bytesLeftInChunk > 0) {
                        readBytes(bytesLeftInChunk);
                    }

                    // update hash with data from buffer
                    digest.add(buffer.data(), static_cast<size_t>(bytesRead));
                }

                return digest.getHash();
            }

            const AppImage* readAppImage(const std::string& pathToAppImage) {
                // error state: empty AppImage path
                if (pathToAppImage.empty()) {
                    issueStatusMessage("Path to AppImage must not be empty.");
                    return nullptr;
                }

                // check whether file exists
                std::ifstream ifs(pathToAppImage);

                // if file can't be opened, it's an error
                if (!ifs || !ifs.good()) {
                    issueStatusMessage("Failed to access AppImage file: " + pathToAppImage);
                    return nullptr;
                }

                // read magic number
                ifs.seekg(8, std::ios::beg);
                std::vector<char> magicByte(4, '\0');
                ifs.read(magicByte.data(), 3);

                // validate first two bytes are A and I
                if (magicByte[0] != 'A' && magicByte[1] != 'I') {
                    std::ostringstream oss;
                    oss << "Invalid magic bytes: " << (int) magicByte[0] << (int) magicByte[1];
                    issueStatusMessage(oss.str());
                }

                int appImageType = -1;
                // the third byte contains the version
                switch (magicByte[2]) {
                    case '\x01':
                        appImageType = 1;
                        break;
                    case '\x02':
                        appImageType = 2;
                        break;
                    default:
                        // see fallback in the final else block in the next if construct
                        break;
                }

                // read update information in the file
                std::string updateInformation;

                // also read signature in the file
                std::string signature;

                if (appImageType == 1) {
                    // update information is always at the same position, and has a fixed length
                    static constexpr auto position = 0x8373;
                    static constexpr auto length = 512;

                    ifs.seekg(position);

                    std::vector<char> rawUpdateInformation(length, 0);
                    ifs.read(rawUpdateInformation.data(), length);

                    updateInformation = rawUpdateInformation.data();
                } else if (appImageType == 2) {
                    // try to read ELF section .upd_info
                    updateInformation = readElfSection(pathToAppImage, ".upd_info");

                    // type 2 supports signatures, so we can extract it here, too
                    signature = readAppImageSignature(pathToAppImage);
                } else {
                    // final try: type 1 AppImages do not have to set the magic bytes, although they should
                    // if the file is both an ELF and an ISO9660 file, we'll suspect it to be a type 1 AppImage, and
                    // proceed with a warning

                    static constexpr int elfMagicPos = 1;
                    static const std::string elfMagicValue = "ELF";

                    static constexpr int isoMagicPos = 32769;
                    static const std::string isoMagicValue = "CD001";

                    ifs.seekg(elfMagicPos);
                    std::vector<char> elfMagicPosData(elfMagicValue.size() + 1, '\0');
                    ifs.read(elfMagicPosData.data(), elfMagicValue.size());
                    auto elfMagicAvailable = (elfMagicPosData.data() == elfMagicValue);

                    ifs.seekg(isoMagicPos);
                    std::vector<char> isoMagicPosData(isoMagicValue.size() + 1, '\0');
                    ifs.read(isoMagicPosData.data(), isoMagicValue.size());
                    auto isoMagicAvailable = (isoMagicPosData.data() == isoMagicValue);

                    if (elfMagicAvailable && isoMagicAvailable) {
                        issueStatusMessage("Guessing AppImage type 1");
                        appImageType = 1;
                    } else {
                        // all possible methods attempted, ultimately fail here
                        issueStatusMessage("No such AppImage type: " + std::to_string(magicByte[2]));
                        return nullptr;
                    }
                }

                UpdateInformationType uiType = INVALID;
                std::string zsyncUrl;

                // parse update information
                auto uiParts = split(updateInformation, '|');

                // make sure uiParts isn't empty
                if (!uiParts.empty()) {
                    // TODO: GitHub releases type should consider pre-releases when there's no other types of releases
                    if (uiParts[0] == "gh-releases-zsync") {
                        // validate update information
                        if (uiParts.size() != 5) {
                            std::ostringstream oss;
                            oss << "Update information has invalid parameter count. Please contact the author of "
                                << "the AppImage and ask them to revise the update information. They should consult "
                                << "the AppImage specification, there might have been changes to the update"
                                <<  "information.";
                            issueStatusMessage(oss.str());
                        } else {
                            uiType = ZSYNC_GITHUB_RELEASES;

                            auto username = uiParts[1];
                            auto repository = uiParts[2];
                            auto tag = uiParts[3];
                            auto filename = uiParts[4];

                            std::stringstream url;
                            url << "https://api.github.com/repos/" << username << "/" << repository << "/releases/";

                            if (tag.find("latest") != std::string::npos) {
                                issueStatusMessage("Fetching latest release information from GitHub API");
                                url << "latest";
                            } else {
                                std::ostringstream oss;
                                oss << "Fetching release information for tag \"" << tag << "\" from GitHub API.";
                                issueStatusMessage(oss.str());
                                url << "tags/" << tag;
                            }

                            auto response = cpr::Get(url.str());

                            // counter that will be evaluated later to give some meaningful feedback why parsing API
                            // response might have failed
                            int downloadUrlLines = 0;
                            int matchingUrls = 0;

                            // continue only if HTTP status is good
                            if (response.status_code >= 200 && response.status_code < 300) {
                                // in contrary to the original implementation, instead of converting wildcards into
                                // all-matching regular expressions, we have the power of fnmatch() available, a real wildcard
                                // implementation
                                // unfortunately, this is still hoping for GitHub's JSON API to return a pretty printed
                                // response which can be parsed like this
                                std::stringstream responseText(response.text);
                                std::string currentLine;

                                // not ideal, but allows for returning a match for the entire line
                                auto pattern = "*" + filename + "*";

                                // iterate through all lines to find a possible download URL and compare it to the pattern
                                while (std::getline(responseText, currentLine)) {
                                    if (currentLine.find("browser_download_url") != std::string::npos) {
                                        downloadUrlLines++;
                                        if (fnmatch(pattern.c_str(), currentLine.c_str(), 0) == 0) {
                                            matchingUrls++;
                                            auto parts = split(currentLine, '"');
                                            zsyncUrl = parts.back();
                                            break;
                                        }
                                    }
                                }
                            } else {
                                issueStatusMessage("GitHub API request failed!");
                            }

                            if (downloadUrlLines <= 0) {
                                std::ostringstream oss;
                                oss << "Could not find any artifacts in release data. "
                                    << "Please contact the author of the AppImage and tell them the files are missing "
                                    <<  "on the releases page.";
                                issueStatusMessage(oss.str());
                            } else if (matchingUrls <= 0) {
                                std::ostringstream oss;
                                oss << "None of the artifacts matched the pattern in the update information. "
                                    << "The pattern is most likely invalid, e.g., due to changes in the filenames of "
                                    << "the AppImages. Please contact the author of the AppImage and ask them to "
                                    << "revise the update information.";
                                issueStatusMessage(oss.str());
                            } else if (zsyncUrl.empty()) {
                                // unlike that this code will ever be reached, the other two messages should cover all
                                // cases in which a ZSync URL is missing
                                // if it does, however, it's most likely that GitHub's API didn't return a URL
                                issueStatusMessage("Failed to parse GitHub's response.");
                            }
                        }
                    } else if (uiParts[0] == "bintray-zsync") {
                        // TODO: better error handling
                        if (uiParts.size() == 5) {
                            uiType = ZSYNC_BINTRAY;

                            auto username = uiParts[1];
                            auto repository = uiParts[2];
                            auto packageName = uiParts[3];
                            auto filename = uiParts[4];

                            std::stringstream downloadUrl;
                            downloadUrl << "https://dl.bintray.com/" << username << "/" << repository << "/"
                                        << filename;

                            if (downloadUrl.str().find("_latestVersion") == std::string::npos) {
                                zsyncUrl = downloadUrl.str();
                            } else {
                                std::stringstream redirectorUrl;
                                redirectorUrl << "https://bintray.com/" << username << "/" << repository << "/"
                                              << packageName << "/_latestVersion";

                                auto versionResponse = cpr::Head(redirectorUrl.str());
                                // this request is supposed to be redirected
                                // due to how cpr works, we can't check for a redirection status, as we get the response for
                                // the redirected request
                                // therefore, we check for a 2xx response, and then can inspect and compare the redirected URL
                                if (versionResponse.status_code >= 200 && versionResponse.status_code < 400) {
                                    auto redirectedUrl = versionResponse.url;

                                    // if they're different, it's probably been successful
                                    if (redirectorUrl.str() != redirectedUrl) {
                                        // the last part will contain the current version
                                        auto packageVersion = static_cast<std::string>(split(redirectedUrl, '/').back());
                                        auto urlTemplate = downloadUrl.str();

                                        // split by _latestVersion, insert correct value, compose final value
                                        auto pos = urlTemplate.find("_latestVersion");

                                        auto firstPart = urlTemplate.substr(0, pos);
                                        auto secondPart = urlTemplate.substr(pos + std::string("_latestVersion").length());
                                        zsyncUrl = firstPart + packageVersion + secondPart;
                                    }
                                }
                            }
                        }
                    } else if (uiParts[0] == "zsync") {
                        // validate update information
                        if (uiParts.size() == 2) {
                            uiType = ZSYNC_GENERIC;

                            zsyncUrl = uiParts.back();
                        } else {
                            std::ostringstream oss;
                            oss << "Update information has invalid parameter count. Please contact the author of "
                                << "the AppImage and ask them to revise the update information. They should consult "
                                << "the AppImage specification, there might have been changes to the update"
                                <<  "information.";
                            issueStatusMessage(oss.str());
                        }
                    } else {
                        // unknown type
                    }
                }

                auto* appImage = new AppImage();

                appImage->filename = pathToAppImage;
                appImage->appImageVersion = appImageType;
                appImage->rawUpdateInformation = updateInformation;
                appImage->updateInformationType = uiType;
                appImage->zsyncUrl = zsyncUrl;
                appImage->signature = signature;

                return appImage;
            }

            bool validateAppImage(AppImage const* appImage) {
                // a null pointer is clearly a sign
                if (appImage == nullptr) {
                    std::ostringstream oss;
                    oss << "Parsing AppImage failed. See previous message for details. "
                        << "Are you sure the file is an AppImage?";
                    issueStatusMessage(oss.str());
                    return false;
                }

                // first check whether there's update information at all
                if (appImage->rawUpdateInformation.empty()) {
                    std::ostringstream oss;
                    oss << "Could not find update information in the AppImage. "
                        << "Please contact the author of the AppImage and ask them to embed update information.";
                    issueStatusMessage(oss.str());
                    return false;
                }

                // now check whether a ZSync URL could be composed by readAppImage
                // this is the only supported update type at the moment
                if (appImage->zsyncUrl.empty()) {
                    std::ostringstream oss;
                    oss << "ZSync URL not available. See previous messages for details.";
                    issueStatusMessage(oss.str());
                    return false;
                }

                // check whether update information is available
                if (appImage->updateInformationType == INVALID) {
                    std::stringstream oss;
                    oss << "Could not detect update information type."
                        << "Please contact the author of the AppImage and ask them whether the update information "
                        << "is correct.";
                    issueStatusMessage(oss.str());
                    return false;
                }

                return true;
            }

            // thread runner
            void runUpdate() {
                // initialization
                {
                    lock_guard guard(mutex);

                    // make sure it runs only once at a time
                    // should never occur, but you never know
                    if (state != INITIALIZED)
                        return;

                    // if there is a ZSync client (e.g., because an update check has been run), clean it up
                    // this ensures that a fresh instance will be used for the update run
                    if (zSyncClient != nullptr) {
                        delete zSyncClient;
                        zSyncClient = nullptr;
                    }


                    // WARNING: if you don't want to shoot yourself in the foot, make sure to read in the AppImage
                    // while locking the mutex and/or before the RUNNING state to make sure readAppImage() finishes
                    // before progress() and such can be called! Otherwise, progress() etc. will return an error state,
                    // causing e.g., main(), to interrupt the thread and finish.
                    auto* appImage = readAppImage(pathToAppImage);

                    if (!validateAppImage(appImage)) {
                        // cleanup
                        delete appImage;

                        state = ERROR;
                        return;
                    }

                    if (appImage->updateInformationType == ZSYNC_BINTRAY) {
                        issueStatusMessage("Updating from Bintray via ZSync");
                    } else if (appImage->updateInformationType == ZSYNC_GITHUB_RELEASES) {
                        issueStatusMessage("Updating from GitHub Releases via ZSync");
                    } else if (appImage->updateInformationType == ZSYNC_GENERIC) {
                        issueStatusMessage("Updating from generic server via ZSync");
                    }

                    if (appImage->updateInformationType == ZSYNC_GITHUB_RELEASES ||
                        appImage->updateInformationType == ZSYNC_BINTRAY ||
                        appImage->updateInformationType == ZSYNC_GENERIC) {
                        // doesn't matter which type it is exactly, they all work like the same
                        zSyncClient = new zsync2::ZSyncClient(appImage->zsyncUrl, pathToAppImage, overwrite);

                        // make sure the new AppImage goes into the same directory as the old one
                        // unfortunately, to be able to use dirname(), one has to copy the C string first
                        auto* path = strdup(appImage->filename.c_str());
                        std::string dirPath = dirname(path);
                        free(path);

                        zSyncClient->setCwd(dirPath);
                    } else {
                        // error unsupported type
                        issueStatusMessage("Error: update method not implemented");

                        // cleanup
                        delete appImage;

                        state = ERROR;
                        return;
                    }

                    // cleanup
                    delete appImage;

                    state = RUNNING;
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

                // TODO: this code is somewhat duplicated in run()
                // should probably be extracted to separate function
                auto* appImage = readAppImage(pathToAppImage);

                // validate AppImage
                if(!validateAppImage(appImage))
                    return false;

                if (appImage->updateInformationType == ZSYNC_GITHUB_RELEASES ||
                    appImage->updateInformationType == ZSYNC_BINTRAY ||
                    appImage->updateInformationType == ZSYNC_GENERIC) {
                    zSyncClient = new zsync2::ZSyncClient(appImage->zsyncUrl, pathToAppImage);
                    return zSyncClient->checkForChanges(updateAvailable, method);
                }

                zSyncClient = nullptr;

                // return error in case of unknown update information
                issueStatusMessage("Unknown update information type, aborting.");
                return false;
            }
        };
        
        Updater::Updater(const std::string& pathToAppImage, bool overwrite) {
            // initialize data class
            d = new Updater::Private();

            // check whether file exists, otherwise throw exception
            std::ifstream f(pathToAppImage);

            if(!f || !f.good()) {
                auto errorMessage = std::strerror(errno);
                throw std::invalid_argument(errorMessage + std::string(": ") + pathToAppImage);
            }

            d->pathToAppImage = pathToAppImage;
            d->overwrite = overwrite;
        }

        Updater::~Updater() {
            delete d;
        }

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

            auto* appImage = d->readAppImage(d->pathToAppImage);

            if (appImage == nullptr)
                return false;

            oss << "Parsing file: " << appImage->filename << std::endl;
            oss << "AppImage type: " << appImage->appImageVersion << std::endl;

            oss << "Raw update information: ";
            if (appImage->rawUpdateInformation.empty())
                oss << "<empty>";
            else
                oss << appImage->rawUpdateInformation;
            oss << std::endl;

            oss << "Update information type: ";

            if (appImage->updateInformationType == d->ZSYNC_GENERIC)
                oss << "Generic ZSync URL";
            else if (appImage->updateInformationType == d->ZSYNC_BINTRAY)
                oss << "ZSync via Bintray";
            else if (appImage->updateInformationType == d->ZSYNC_GITHUB_RELEASES)
                oss << "ZSync via GitHub Releases";
            else if (appImage->updateInformationType == d->INVALID)
                oss << "Invalid (parsing failed/no update information available)";
            else
                oss << "Unknown error";

            oss << std::endl;

            if (!appImage->zsyncUrl.empty())
                oss << "Assembled ZSync URL: " << appImage->zsyncUrl << std::endl;
            else
                oss << "Failed to assemble ZSync URL. AppImageUpdate can not be used with this AppImage.";

            description = oss.str();

            return true;
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

            auto pathToOldAppImage = d->pathToAppImage;
            if (pathToOldAppImage == pathToNewAppImage) {
                pathToOldAppImage = pathToNewAppImage + ".zs-old";
            }

            auto oldSignature = d->readAppImageSignature(d->pathToAppImage);
            auto newSignature = d->readAppImageSignature(pathToNewAppImage);

            // remove any spaces and/or newline characters there might be on the left or right of the payload
            zsync2::trim(oldSignature, '\n');
            zsync2::trim(newSignature, '\n');
            zsync2::trim(oldSignature);
            zsync2::trim(newSignature);

            auto oldSigned = !oldSignature.empty();
            auto newSigned = !newSignature.empty();

            auto oldDigest = d->hashAppImage(pathToOldAppImage);
            auto newDigest = d->hashAppImage(pathToNewAppImage);

            auto oldDigestOrig = readElfSection(pathToOldAppImage, ".sha256_sig");
            auto newDigestOrig = readElfSection(pathToNewAppImage, ".sha256_sig");

            std::string tempDir;
            {
                char* buffer;

                char* pattern = strdup("/tmp/AppImageUpdate-XXXXXX");
                if ((buffer = mkdtemp(pattern)) == nullptr) {
                    d->issueStatusMessage("Failed to create temporary directory");
                    return VALIDATION_TEMPDIR_CREATION_FAILED;
                }

                tempDir = buffer;

                free(pattern);
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
                {VALIDATION_NOT_SIGNED, "AppImages not signed"},
                {VALIDATION_KEY_CHANGED, "Key changed for signing AppImages"},

                // error states
                {VALIDATION_FAILED, "Signature validation failed"},
                {VALIDATION_GPG2_MISSING, "gpg2 command not found, please install"},
                {VALIDATION_GPG2_CALL_FAILED, "Call to gpg2 failed"},
                {VALIDATION_TEMPDIR_CREATION_FAILED, "Failed to create temporary directory"},
                {VALIDATION_NO_LONGER_SIGNED, "AppImage no longer comes with signature"},
                {VALIDATION_BAD_SIGNATURE, "Bad signature"},
            };

            if (validationMessages.count(state) > 0) {
                return validationMessages.at(state);
            }

            return "Unknown validation state";
        }

        std::string Updater::updateInformation() const {
            const auto* appImage = d->readAppImage(d->pathToAppImage);

            if (appImage == nullptr)
                throw std::runtime_error("Failed to parse AppImage");

            return appImage->rawUpdateInformation;
        }
    }
}
