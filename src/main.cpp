// system headers
#include <chrono>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <thread>
#include <unistd.h>
#include <optional>

// library headers
#include <argagg/argagg.hpp>

// local headers
#include "appimage/update.h"
#include "util.h"

using namespace std;
using namespace appimage::update;
using namespace appimage::update::util;

int main(const int argc, const char** argv) {
    argagg::parser parser{{
        {"help", {"-h", "--help"}, "Display this help text."},
        {"version", {"-V", "--version"}, "Display version information."},
        {"describe", {"-d", "--describe"}, "Parse and describe AppImage and its update information and exit."},
        {"checkForUpdate", {"-j", "--check-for-update"}, "Check for update. Exits with code 1 if changes are available, 0 if there are not,"
                                                         "other non-zero code in case of errors."},
        {"overwriteOldFile", {"-O", "--overwrite"}, "Overwrite existing file. If not specified, a new file will be created, and the old one will remain untouched."},
        {"removeOldFile", {"-r", "--remove-old"}, "Remove old AppImage after successful update."},
        {"selfUpdate", {"--self-update"}, "Update this AppImage."},
    }};

    argagg::parser_results args;

    try {
        args = parser.parse(argc, argv);
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return EXIT_FAILURE;
    }

    const auto showUsage = [argv, &parser]() {
        std::cerr << "AppImage companion tool taking care of updates for the commandline." << endl << endl;
        cerr << "Usage: " << argv[0] << " [options...] [<path to AppImage>]" << endl << endl;
        cerr << parser;
    };

    if (args["help"]) {
        showUsage();
        return EXIT_SUCCESS;
    }

    if (args["version"]) {
        cerr << "appimageupdatetool version " << APPIMAGEUPDATE_VERSION
             << " (commit " << APPIMAGEUPDATE_GIT_COMMIT << "), "
             << "build " << BUILD_NUMBER << " built on " << BUILD_DATE << endl;
        return 0;
    }

    optional<string> pathToAppImage = [&args]() {
        if (!args.pos.empty()) {
            return optional<string>(args.as<string>(0));
        }

        return optional<string>();
    }();

    // if a self-update is requested, check whether the path argument has been passed, and show an error
    // otherwise check whether path has been passed on the CLI, otherwise show file chooser
    if (args["selfUpdate"]) {
        if (pathToAppImage.has_value()) {
            cerr << "Error: --self-update does not take a path." << endl;
            showUsage();
            return EXIT_FAILURE;
        } else {
            auto* APPIMAGE = getenv("APPIMAGE");

            if (APPIMAGE == nullptr) {
                cerr << "Error: self update requested but could not determine path to AppImage "
                     << "($APPIMAGE environment variable missing)."
                     << endl;
                return 1;
            }

            if (!isFile(APPIMAGE)) {
                cerr << "Error: $APPIMAGE pointing to non-existing file:\n"
                     << APPIMAGE << endl;
                return 1;
            }

            pathToAppImage = APPIMAGE;
        }
    }
    
    if (!pathToAppImage.has_value()) {
        showUsage();
        return EXIT_FAILURE;
    }

    // after checking that a path is given, check whether the file actually exists
    if (!isFile(pathToAppImage.value())) {
        // cannot tell whether it exists or not without inspecting errno, therefore using a more generic error message
        cerr << "Could not read file: " << pathToAppImage.value();
        return 1;
    }

    Updater updater(pathToAppImage.value(), args["overwriteOldFile"]);

    // if the user just wants a description of the AppImage, parse the AppImage, print the description and exit
    if (args["describe"]) {
        string description;

        if (!updater.describeAppImage(description)) {
            // TODO: better description of what went wrong
            cerr << description << endl;
            cerr << "Failed to parse AppImage. See above for more information" << endl;
            return 1;
        }

        // post all status messages on stderr...
        {
            std::string nextMessage;
            while (updater.nextStatusMessage(nextMessage))
                cerr << nextMessage << endl;
        }
        // ... insert an empty line to separate description and messages visually ...
        cerr << endl;

        // ... and the description on stdout
        cout << description;

        return 0;
    }

    if (args["checkForUpdate"]) {
        bool changesAvailable = false;

        auto result = updater.checkForChanges(changesAvailable);

        // print all messages that might be available
        {
            std::string nextMessage;
            while (updater.nextStatusMessage(nextMessage))
                cerr << nextMessage << endl;
        }

        if (!result) {
            cerr << "Error checking for changes!";
            return 2;
        }

        return changesAvailable ? 1 : 0;
    }

    // first of all, check whether an update is required at all
    // this avoids unnecessary file I/O (a real update process would create a copy of the file anyway in case an
    // update is not required)
    cout << "Checking for updates..." << endl;
    bool updateRequired = true;

    auto updateCheckSuccessful = updater.checkForChanges(updateRequired);

    // fetch messages from updater before showing any error messages, giving the user a chance to check for errors
    {
        std::string nextMessage;
        while (updater.nextStatusMessage(nextMessage))
            cout << nextMessage << endl;
    }

    if (!updateCheckSuccessful) {
        cerr << "Update check failed, exiting!" << endl;
        return 2;
    }

    cout << "... done!" << endl;
    if (!updateRequired) {
        cout << "Update not required, exiting." << endl;
        return 0;
    }

    // to be fair, this check is not really required (why should this fail), but for the sake of completeness, it's
    // provided here
    if(!updater.start()) {
        cerr <<  "Start failed!" << endl;
        return 1;
    }

    cerr << "Starting update..." << endl;

    while(!updater.isDone()) {
        this_thread::sleep_for(chrono::milliseconds(100));
        double progress;

        bool firstMessage = true;
        std::string nextMessage;
        while (updater.nextStatusMessage(nextMessage)) {
            if (firstMessage)
                cout << endl;
            firstMessage = false;

            cout << nextMessage << endl;
        }

        if (!updater.progress(progress))
            return 1;

        off_t fileSize = 0;
        if (!updater.remoteFileSize(fileSize))
            fileSize = -1;

        double fileSizeInMiB = fileSize / 1024.0f / 1024.0f;

        cout << "\33[2K\r" << (progress * 100.0f) << "% done";
        if (fileSize >= 0)
            cout << fixed << setprecision(2) << " (" << progress * fileSizeInMiB << " of " << fileSizeInMiB << " MiB)...";
        cout << flush;
    }

    std::string nextMessage;
    while (updater.nextStatusMessage(nextMessage))
        cout << nextMessage << endl;

    cout << endl;

    if(updater.hasError()) {
        cerr << "Update failed!" << endl;
        return 1;
    }

    string newFilePath;

    // really shouldn't fail here any more, but just in case...
    if (!updater.pathToNewFile(newFilePath)) {
        cerr << "Fatal error: could not determine path to new file!" << endl;
        return 1;
    }

    auto validationResult = updater.validateSignature();

    while (updater.nextStatusMessage(nextMessage))
        cout << nextMessage << endl;

    auto oldFilePath = pathToOldAppImage(pathToAppImage.value(), newFilePath);

    if (validationResult >= Updater::VALIDATION_FAILED) {
        // validation failed, restore original file to prevent bad things from happening
        updater.restoreOriginalFile();

        cerr << "Validation error: " << Updater::signatureValidationMessage(validationResult) << endl
             << "Restoring original file" << endl;

        return 1;
    }

    if (validationResult >= Updater::VALIDATION_WARNING) {
        if (validationResult == Updater::VALIDATION_NOT_SIGNED) {
            // copy permissions of the old AppImage to the new version
            updater.copyPermissionsToNewFile();
        }

        cerr << "Validation warning: " << Updater::signatureValidationMessage(validationResult) << endl;
    }

    if (validationResult <= Updater::VALIDATION_WARNING) {
        // copy permissions of the old AppImage to the new version
        updater.copyPermissionsToNewFile();
        cerr << "Signature validation passed" << endl;
    }

    if (args["removeOldFile"]) {
        if (isFile(oldFilePath)) {
            cerr << "Removing old AppImage: " << oldFilePath << endl;
            unlink(oldFilePath.c_str());
        } else {
            cerr << "Warning: could not find old AppImage: " << oldFilePath << endl;
        }
    }

    cerr << "Update successful. "
         << (args["overwriteOldFile"] ? "Updated existing file " : "New file created: ") << newFilePath
         << endl;

    return 0;
}
