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

    // for now, the application serves as an API demo
    // it is not too advanced.

    appimage::update::Updater updater(argv[1]);

    cerr << "Starting update..." << endl;

    // to be fair, this check is not really required (why should this fail), but for the sake of completeness, it's
    // provided here
    if(!updater.start()) {
        cerr <<  "Start failed!" << endl;
        return 1;
    }

    while(!updater.isDone()) {
        this_thread::sleep_for(chrono::milliseconds(100));
        double progress;

        std::string nextMessage;
        while (updater.nextStatusMessage(nextMessage))
            cout << "\r" << nextMessage << endl;

        if (!updater.progress(progress))
            return 1;

        cout << "\33[2K\r" << (progress * 100.0f) << "% done..." << flush;
    }

    cerr << endl;

    if(updater.hasError()) {
        cerr << "Update failed!" << endl;
        return 1;
    }

    cerr << "Update successful!" << endl;

    return 0;
}
