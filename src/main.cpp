// system headers
#include <chrono>
#include <iostream>
#include <thread>

// library headers
#include <args.hxx>

// local headers
#include "appimage/update.h"
#include "util.h"

using namespace std;
using namespace appimage::update;

int main(const int argc, const char** argv) {
    args::ArgumentParser parser("AppImage companion tool taking care of updates for the commandline.");

    args::Flag showVersion(parser, "", "Display version and exit.", {'V', "version"});

    args::Flag describeAppImage(parser, "",
        "Parse and describe AppImage and its update information and exit.",
        {'d', "describe"}
    );
    args::Positional<std::string> pathToAppImage(parser, "path", "path to AppImage");

    try {
        parser.ParseCLI(argc, argv);
    } catch (args::Help&) {
        cerr << parser;
        return 0;
    } catch (args::ParseError& e) {
        std::cerr << e.what() << std::endl;
        std::cerr << parser;
        return 1;
    }

    if (showVersion) {
        cerr << "AppImageUpdate version " << APPIMAGEUPDATE_VERSION
             << " (commit " << APPIMAGEUPDATE_GIT_COMMIT << "), "
             << "build " << BUILD_NUMBER << " built on " << BUILD_DATE << endl;
        return 0;
    }

    if (!pathToAppImage) {
        cerr << parser;
        return 0;
    }

    // after checking that a path is given, check whether the file actually exists
    if (isFile(pathToAppImage.Get())) {
        // cannot tell whether it exists or not without inspecting errno, therefore using a more generic error message
        cerr << "Could not read file: " << pathToAppImage;
    }

    Updater updater(pathToAppImage.Get());

    // if the user just wants a description of the AppImage, parse the AppImage, print the description and exit
    if (describeAppImage) {
        string description;

        if (!updater.describeAppImage(description)) {
            // TODO: better description of what went wrong
            cerr << "Failed to parse AppImage!" << endl;
            return 1;
        }

        cout << description;
        return 0;
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

        cout << "\33[2K\r" << (progress * 100.0f) << "% done..." << flush;
    }

    std::string nextMessage;
    while (updater.nextStatusMessage(nextMessage))
        cout << nextMessage << endl;

    cout << endl;

    if(updater.hasError()) {
        cerr << "Update failed!" << endl;
        return 1;
    }

    cerr << "Update successful!" << endl;

    return 0;
}
