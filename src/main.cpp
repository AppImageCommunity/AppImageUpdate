// system headers
#include <chrono>
#include <iostream>
#include <thread>

// local headers
#include "appimage/update.h"

using namespace std;

static void showUsage(const string& argv0) {
    cout << "Usage: " << argv0 << " <path to AppImage>" << endl;
}

int main(const int argc, const char** argv) {
    if(argc <= 1) {
        showUsage(argv[0]);
        return 1;
    }

    cerr << "AppImageUpdate version " << APPIMAGEUPDATE_VERSION << " (commit " << APPIMAGEUPDATE_GIT_COMMIT << "), "
         << "build " << BUILD_NUMBER << " built on " << BUILD_DATE << endl;

    appimage::update::Updater updater(argv[1]);

    // first of all, check whether an update is required at all
    // this avoids unnecessary file I/O (a real update process would create a copy of the file anyway in case an
    // update is not required)
    cout << "Checking for updates..." << endl;
    bool updateRequired;

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
