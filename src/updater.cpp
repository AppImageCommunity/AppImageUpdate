// system headers+
#include <algorithm>
#include <chrono>
#include <deque>
#include <fnmatch.h>
#include <fstream>
#include <iostream>
#include <mutex>
#include <sstream>
#include <thread>

// library headers
#include <zsclient.h>
#include <cpr/cpr.h>

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
                            mutex() {};

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

                AppImage() : appImageVersion(-1), updateInformationType(INVALID) {};
            };
            typedef struct AppImage AppImage;

        public:
            void issueStatusMessage(std::string message) {
                statusMessages.push_back(message);
            }

            static const AppImage* readAppImage(const std::string pathToAppImage) {
                // error state: empty AppImage path
                if (pathToAppImage.empty())
                    return nullptr;

                // check whether file exists
                std::ifstream ifs(pathToAppImage);

                // if file can't be opened, it's an error
                if (!ifs || !ifs.good())
                    return nullptr;

                // read magic number
                ifs.seekg(8, std::ios::beg);
                unsigned char magicByte[4] = {0, 0, 0, 0};
                ifs.read((char*) magicByte, 3);

                // validate first two bytes are A and I
                if (magicByte[0] != 'A' || magicByte[1] != 'I')
                    return nullptr;

                uint8_t version;
                // the third byte contains the version
                switch (magicByte[2]) {
                    case '\x01':
                        version = 1;
                        break;
                    case '\x02':
                        version = 2;
                        break;
                    default:
                        return nullptr;
                }

                // read update information in the file
                std::string updateInformation;

                if (version == 1) {
                    // TODO implement type 1 update information parser
                } else if (version == 2) {
                    // check whether update information can be found inside the file by calling objdump
                    auto command = "objdump -h \"" + pathToAppImage + "\"";

                    std::string match;
                    if (!callProgramAndGrepForLine(command, ".upd_info", match))
                        return nullptr;

                    auto parts = split(match);
                    parts.erase(std::remove_if(parts.begin(), parts.end(),
                        [](std::string s) { return s.length() <= 0; }
                    ));

                    auto offset = std::stoi(parts[5], nullptr, 16);
                    auto length = std::stoi(parts[2], nullptr, 16);

                    ifs.seekg(offset, std::ios::beg);
                    auto* rawUpdateInformation = static_cast<char*>(calloc(length, sizeof(char)));
                    ifs.read(rawUpdateInformation, length);

                    updateInformation = rawUpdateInformation;
                    free(rawUpdateInformation);
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
                        if (uiParts.size() == 5) {
                            uiType = ZSYNC_GITHUB_RELEASES;

                            auto username = uiParts[1];
                            auto repository = uiParts[2];
                            auto tag = uiParts[3];
                            auto filename = uiParts[4];

                            std::stringstream url;
                            url << "https://api.github.com/repos/" << username << "/" << repository << "/releases/";

                            if (tag.find("latest") != std::string::npos) {
                                url << "latest";
                            } else {
                                url << "tags/" << tag;
                            }

                            auto response = cpr::Get(url.str());

                            // continue only if HTTP status is good
                            if (response.status_code >= 200 && response.status_code < 300) {
                                // in contrary to the original implementation, instead of converting wildcards into
                                // all-matching regular expressions, we have the power of fnmatch() available, a real wildcard
                                // implementation
                                // unfortunately, this is still hoping for GitHub's JSON API to return a pretty printed
                                // response which can be parsed like this
                                std::stringstream responseText(response.text);
                                std::string currentLine;
                                auto pattern = "*" + filename + "*";
                                while (std::getline(responseText, currentLine)) {
                                    if (currentLine.find("browser_download_url") != std::string::npos) {
                                        if (fnmatch(pattern.c_str(), currentLine.c_str(), 0) == 0) {
                                            auto parts = split(currentLine, '"');
                                            zsyncUrl = parts.back();
                                            break;
                                        }
                                    }
                                }
                            }
                        }
                    } else if (uiParts[0] == "bintray-zsync") {
                        if (uiParts.size() == 5) {
                            uiType = ZSYNC_BINTRAY;

                            auto username = uiParts[1];
                            auto repository = uiParts[2];
                            auto packageName = uiParts[3];
                            auto filename = uiParts[4];

                            std::stringstream downloadUrl;
                            downloadUrl << "https://dl.bintray.com/" << username << "/" << repository << "/"
                                        << filename;

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
                    } else if (uiParts[0] == "zsync") {
                        // validate update information
                        if (uiParts.size() == 2) {
                            uiType = ZSYNC_GENERIC;

                            zsyncUrl = uiParts.back();
                        }
                    } else {
                        // unknown type
                    }
                }

                auto* appImage = new AppImage();

                appImage->filename = pathToAppImage;
                appImage->appImageVersion = version;
                appImage->rawUpdateInformation = updateInformation;
                appImage->updateInformationType = uiType;
                appImage->zsyncUrl = zsyncUrl;

                return appImage;
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

                    // WARNING: if you don't want to shoot yourself in the foot, make sure to read in the AppImage
                    // while locking the mutex and/or before the RUNNING state to make sure readAppImage() finishes
                    // before progress() and such can be called! Otherwise, progress() etc. will return an error state,
                    // causing e.g., main(), to interrupt the thread and finish.
                    auto* appImage = readAppImage(pathToAppImage);

                    if (appImage == nullptr) {
                        issueStatusMessage("Parsing failed! Are you sure the file is an AppImage?");
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
                    if (!appImage->zsyncUrl.empty())
                        issueStatusMessage("Update URL: " + appImage->zsyncUrl);
                    else {
                        issueStatusMessage("Could not find update URL!");

                        if (appImage->updateInformationType == ZSYNC_GITHUB_RELEASES)
                            issueStatusMessage("Please beware that pre-releases are not considered by the GitHub "
                                               "releases update information type!");

                        state = ERROR;
                        return;
                    }

                    // check whether update information is available
                    if (appImage->updateInformationType == INVALID) {
                        state = ERROR;
                        return;
                    }

                    if (appImage->updateInformationType == ZSYNC_GITHUB_RELEASES ||
                        appImage->updateInformationType == ZSYNC_BINTRAY ||
                        appImage->updateInformationType == ZSYNC_GENERIC) {
                        // doesn't matter which type it is exactly, they all work like the same
                        zSyncClient = new zsync2::ZSyncClient(appImage->zsyncUrl, pathToAppImage);
                    } else {
                        // error unsupported type
                        state = ERROR;
                        return;
                    }

                    state = RUNNING;

                    // cleanup
                    delete appImage;
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

                if (appImage == nullptr) {
                    issueStatusMessage("Parsing failed! Are you sure the file is an AppImage?");
                    return false;
                }

                if (appImage->zsyncUrl.empty()) {
                    issueStatusMessage("Could not find or parse update information in the AppImage! "
                                       "Please contact the author to embed update information!");
                    return false;
                }

                if (appImage->updateInformationType == ZSYNC_GITHUB_RELEASES ||
                    appImage->updateInformationType == ZSYNC_BINTRAY ||
                    appImage->updateInformationType == ZSYNC_GENERIC) {
                    auto client = zsync2::ZSyncClient(appImage->zsyncUrl, pathToAppImage);
                    return client.checkForChanges(updateAvailable, method);
                }

                // return error in case of unknown update information
                return false;
            }
        };
        
        Updater::Updater(const std::string& pathToAppImage) {
            // initialize data class
            d = new Updater::Private();

            // check whether file exists, otherwise throw exception
            std::ifstream f(pathToAppImage);

            if(!f || !f.good()) {
                auto errorMessage = std::strerror(errno);
                throw std::invalid_argument(errorMessage + std::string(": ") + pathToAppImage);
            }

            d->pathToAppImage = pathToAppImage;
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
                progress = 0;
                return true;
            } else if (d->state == SUCCESS || d->state == ERROR) {
                progress = 1;

                delete d->zSyncClient;
                d->zSyncClient = nullptr;

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

        bool Updater::describeAppImage(std::string& description) {
            std::ostringstream oss;

            auto* appImage = d->readAppImage(d->pathToAppImage);

            if (appImage == nullptr)
                return false;

            oss << "Parsing file: " << appImage->filename << std::endl;
            oss << "AppImage type: " << appImage->appImageVersion << std::endl;
            oss << "Raw update information: " << appImage->rawUpdateInformation << std::endl;

            oss << "Update information type: ";

            if (appImage->updateInformationType == d->ZSYNC_GENERIC)
                oss << "Generic ZSync URL";
            else if (appImage->updateInformationType == d->ZSYNC_BINTRAY)
                oss << "ZSync via Bintray";
            else if (appImage->updateInformationType == d->ZSYNC_GITHUB_RELEASES)
                oss << "ZSync via GitHub Releases";
            else if (appImage->updateInformationType == d->INVALID)
                oss << "Invalid (usually means that it couldn't be parsed)";
            else
                oss << "Unknown error";

            oss << std::endl;

            if (!appImage->zsyncUrl.empty())
                oss << "Assembled ZSync URL: " << appImage->zsyncUrl << std::endl;

            description = oss.str();

            return true;
        }
    }
}
