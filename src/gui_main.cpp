// system headers
#include <chrono>
#include <iterator>
#include <iostream>
#include <thread>
#include <unistd.h>

// library headers
#include <desktopenvironments.h>
#include <FL/Fl.H>
#include <FL/Fl_Native_File_Chooser.H>
#include <FL/Fl_Progress.H>
#include <FL/Fl_Text_Buffer.H>
#include <FL/Fl_Text_Display.H>
#include <FL/Fl_Window.H>

// local headers
#include "appimage/update.h"
#include "util.h"

using namespace std;
using namespace appimage::update;

// used to detect whether application is done from other functions
static bool ERROR = false;

void setFltkFont(const std::string &font) {
    // font could contain a size, which has to be parsed out
    auto fontParts = split(font);

    long fontSize = 0;
    if (!toLong(fontParts.back(), fontSize) || fontSize < 6)
        fontSize = 10;
    else
        fontParts.pop_back();

    // check for font slant
    bool italic = false, bold = false;

    while (true) {
         auto fontSlant = toLower(fontParts.back());
        if (fontSlant == "regular") {
            // nothing special, doesn't need to be handled further
            fontParts.pop_back();
        } else if (fontSlant == "roman"
                   || fontSlant == "oblique"
                   || fontSlant == "light"
                   || fontSlant == "demi-bold"
                   || fontSlant == "medium"
                   || fontSlant == "Black"
            ) {
            // unsupported flags, skipping
            fontParts.pop_back();
        } else if (fontSlant == "italic") {
            italic = true;
            fontParts.pop_back();
        } else if (fontSlant == "bold") {
            bold = true;
            fontParts.pop_back();
        } else {
            // no font style found, breaking out
            break;
        }
    }

    ostringstream realFont;

    // FLTK interprets these font prefixes
    if (italic && bold)
        realFont << "P ";
    else if (italic)
        realFont << "I";
    else if (bold)
        realFont << "B ";

    copy(fontParts.begin(), fontParts.end(), std::ostream_iterator<string>(realFont, " "));

    auto finalFont = realFont.str();
    trim(finalFont);

    // TODO: find way to set font size
    Fl::set_font(FL_HELVETICA, strdup(finalFont.c_str()));
}

void windowCallback(Fl_Widget* widget, void*) {
    if (ERROR) {
        exit(0);
    }
}

bool isFile(const std::string& path) {
    struct stat buffer;
    return stat(path.c_str(), &buffer) == 0;
}

// to be run in a thread
void runUpdate(const std::string pathToAppImage) {
    auto runApp = [&pathToAppImage]() {
        // check existing permissions
        struct stat appImageStat;

        if (stat(pathToAppImage.c_str(), &appImageStat) != 0) {
            int error = errno;
            ostringstream ss;
            ss << "Error calling stat(): " << strerror(error);
            fl_message("%s", ss.str().c_str());
            exit(1);
        }

        // make executable
        chmod(pathToAppImage.c_str(), appImageStat.st_mode + S_IXUSR);

        // full path to AppImage, required for execl
        char* realPathToAppImage;
        if ((realPathToAppImage = realpath(pathToAppImage.c_str(), nullptr)) == nullptr) {
            auto error = errno;
            cerr << "Error resolving full path of AppImage: code " << error << ": " << strerror(error) << endl;
            exit(1);
        }

        if (fork() == 0) {
            putenv(strdup("STARTED_BY_APPIMAGEUPDATE=1"));

            cerr << "Running " << realPathToAppImage << endl;

            // make sure to deactivate updater contained in the AppImage when running from AppImageUpdate
            execl(realPathToAppImage, realPathToAppImage, nullptr);

            // execle should never return, so if this code is reached, there must be an error
            auto error = errno;
            cerr << "Error executing AppImage " << realPathToAppImage << ": code " << error << ": "
                 << strerror(error) << endl;
            exit(1);
        }
    };

    if (!isFile(pathToAppImage)) {
        fl_alert("No such file or directory: %s", pathToAppImage.c_str());
        exit(1);
    }

    static const auto winWidth = 500;
    static const auto winHeight = 300;

    Fl_Window win(winWidth, winHeight, "AppImageUpdate GUI");
    win.begin();

    Fl_Progress progressBar(50, winHeight-30, winWidth-(50*2), 20, "0%");

    Fl_Text_Display textDisplay(10, 10, winWidth-(2*10), winHeight-50);
    Fl_Text_Buffer textBuffer;
    textDisplay.buffer(textBuffer);

    win.callback(windowCallback);
    win.end();
    win.show();

    auto log = [&textDisplay, &textBuffer](const string& msg) {
        ostringstream message;
        message << msg << endl;

        cout << message.str();

        textBuffer.insert(textBuffer.length() + 1, message.str().c_str());
        textDisplay.scroll(INT_MAX, 0);

        Fl::check();
    };

    auto showFinishedDialog = [&runApp](string msg) {
        switch (fl_choice(msg.c_str(), "Exit now.", "Run app!", nullptr)) {
            case 0:
                exit(0);
            case 1: {
                runApp();
            }
        }
    };

    Updater updater(pathToAppImage);

    // first of all, check whether an update is required at all
    // this avoids unnecessary file I/O (a real update process would create a copy of the file anyway in case an
    // update is not required)
    log("Checking for updates...");
    bool updateRequired;
    if (updater.checkForChanges(updateRequired)) {
        static const string selfUpdateBinary = "appimageupdategui-selfupdate";

        ostringstream typeCommand;
        typeCommand << "type " << selfUpdateBinary << " 2>&1 1>/dev/null";

        auto x = typeCommand.str();

        // check whether self update is possible
        if (getenv("APPIMAGE") != nullptr || getenv("APPDIR") != nullptr || system(typeCommand.str().c_str()) != 0)
            exit(1);

        switch (fl_choice("Update check failed!\nDo you want to look for a newer version of AppImageUpdate?",
                          "Check for updates", "Exit now", nullptr)) {
            case 0:
                execl("appimageupdategui-selfupdate", "appimageupdategui-selfupdate");
                // if exec will return, it's an error
                cerr << "Failed to call " << selfUpdateBinary << endl;
                exit(2);
            case 1:
                exit(1);
        }
    }
    log("... done!");
    if (!updateRequired) {
        showFinishedDialog("AppImage already up to date, no update required!\n"
                           "Do you want to run the application right now?");
        exit(0);
    }

    log("Starting update...");
    if(!updater.start()) {
        log("Failed to start update process!");
        ERROR = true;
        return;
    }

    double oldProgress = 0;

    while (!updater.isDone()) {
        this_thread::sleep_for(chrono::milliseconds(100));

        double progress;
        if (!updater.progress(progress)) {
            log("Call to progress() failed!");
            ERROR = true;
            return;
        }

        progress *= 100;

        // check for change to avoid having to redraw every 100ms
        if (progress != oldProgress) {
            progressBar.value(static_cast<float>(progress));

            ostringstream label;
            label << progress << "%";
            progressBar.label(label.str().c_str());

            // update UI
            Fl::check();
        }

        std::string nextMessage;
        while (updater.nextStatusMessage(nextMessage))
            log(nextMessage);
    }

    if (updater.hasError()) {
        log("Update failed!");
        progressBar.selection_color(FL_RED);
        progressBar.redraw();
        Fl::check();
        fl_alert("Update failed!");
        exit(0);
    } else {
        progressBar.selection_color(FL_GREEN);
        progressBar.redraw();
        Fl::check();
        log("Successfully updated AppImage!");
    }

    auto oldFile = pathToAppImage + ".zs-old";
    if (isFile(oldFile)) {
        log("Removing backup " + oldFile);
        unlink(oldFile.c_str());
    }

#ifdef SELFUPDATE
    runApp();
#else
    showFinishedDialog("Update successful!\nDo you want to run the application right now?");
#endif

    // trigger exit to avoid FLTK warnings
    exit(0);
}

int main(const int argc, const char* const* argv) {
    std::string pathToAppImage;

#ifdef SELFUPDATE
    {
        auto *envVar = getenv("APPIMAGE");
        if (envVar == nullptr) {
            cerr << "Fatal: APPIMAGE environment variable not set!" << endl;
            exit(2);
        }
        pathToAppImage = envVar;
    }
#else
    // check whether path to AppImage has been passed on the CLI, otherwise show file chooser
    if (argc < 2) {
        Fl_Native_File_Chooser fileChooser(Fl_Native_File_Chooser::BROWSE_FILE);
        fileChooser.title("Please choose an AppImage for updating");
        fileChooser.filter("*.{appimage,AppImage}");

        switch (fileChooser.show()) {
            case 0: {
                const auto *directoryPath = fileChooser.directory();
                const auto *filenamePath = fileChooser.filename();

                ostringstream path;

                if (directoryPath != nullptr) {
                    path << directoryPath;
                    if (directoryPath[strlen(directoryPath) - 1] != '/')
                        path << '/';
                }

                if (filenamePath != nullptr)
                    path << filenamePath;

                pathToAppImage = path.str();
                break;
            }
            case 1:
                // exit silently
                exit(1);
            case -1:
                fl_message("Error while selecting file: %s", fileChooser.errmsg());
                exit(1);
            default:
                fl_message("Fatal error!");
                exit(1);
        }
    } else {
        pathToAppImage = argv[1];
    }
#endif

    IDesktopEnvironment* desktopEnvironment = IDesktopEnvironment::getInstance();

    std::string font;

    if (desktopEnvironment != nullptr
        && desktopEnvironment->gtkInterfaceFont(font)) {
        setFltkFont(font);
    }

    // run worker thread so UI can run in main thread
    thread workerThread(runUpdate, pathToAppImage);

    auto result = Fl::run();

    // wait for worker thread
    workerThread.join();

    delete desktopEnvironment;
    return result;
}
