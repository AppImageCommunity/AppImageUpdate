// system headers
#include <chrono>
#include <iterator>
#include <iostream>
#include <thread>
#include <unistd.h>

// library headers
#include <args.hxx>
#include <desktopenvironments.h>
#include <FL/Fl.H>
#include <FL/Fl_Native_File_Chooser.H>
#include <FL/Fl_Progress.H>
#include <FL/Fl_Text_Buffer.H>
#include <FL/Fl_Text_Display.H>
#include <FL/Fl_Window.H>
#include <X11/xpm.h>
#include <zsutil.h>

// local headers
#include "appimage/update.h"
#include "util.h"

using namespace std;
using namespace appimage::update;

// name of variable that is set and evaluated when AppImageUpdate needs to elevate to root
static const std::string ELEVATED_VAR = "APPIMAGEUPDATE_ELEVATED_TO_ROOT";

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

// to be run in a thread
// caution: using a reference instead of copying the value doesn't work
void runUpdate(const std::string pathToAppImage) {
    auto runApp = [](const string& path, uid_t uid) {
        // make executable
        mode_t newPerms;
        auto errCode = zsync2::getPerms(path, newPerms);

        if (errCode != 0) {
            ostringstream ss;
            ss << "Error calling stat(): " << strerror(errCode);
            fl_message("%s", ss.str().c_str());
            exit(1);
        }

        chmod(path.c_str(), newPerms | S_IXUSR);

        // full path to AppImage, required for execl
        char* realPathToAppImage;
        if ((realPathToAppImage = realpath(path.c_str(), nullptr)) == nullptr) {
            auto error = errno;
            cerr << "Error resolving full path of AppImage: code " << error << ": " << strerror(error) << endl;
            exit(1);
        }

        auto forkResult = fork();

        if (forkResult == 0) {
            // change user if necessary
            if (uid != getuid()) {
                cerr << "Dropping privileges to user " << uid << endl;
                setuid(uid);
                setgid((gid_t) gidForUid(uid));
            }

            putenv(strdup("STARTED_BY_APPIMAGEUPDATE=1"));

            cerr << "Running " << realPathToAppImage << endl;

            // make sure to deactivate updater contained in the AppImage when running from AppImageUpdate
            execl(realPathToAppImage, realPathToAppImage, nullptr);

            // execle should never return, so if this code is reached, there must be an error
            auto error = errno;
            ostringstream oss;
            oss << "Error executing AppImage " << realPathToAppImage << ":\n"
                << "code " << error << ": " << strerror(error) << endl;
            fl_alert(oss.str().c_str());
            exit(1);
        } else if (forkResult > 0) {
            return;
        } else {
            auto error = errno;
            cerr << "fork() failed: " << strerror(error) << endl;
            exit(3);
        }
    };

    if (!isFile(pathToAppImage)) {
        fl_alert("Could not access file: %s", pathToAppImage.c_str());
        exit(1);
    }

    static const auto winWidth = 500;
    static const auto winHeight = 300;

    Fl_Window win(winWidth, winHeight, "AppImageUpdate");
    win.begin();

    Fl_Progress progressBar(50, winHeight-30, winWidth-(50*2), 20, "0%");

    Fl_Text_Display textDisplay(10, 10, winWidth-(2*10), winHeight-50);
    Fl_Text_Buffer textBuffer;
    textDisplay.buffer(textBuffer);
    textDisplay.wrap_mode(Fl_Text_Display::WRAP_AT_BOUNDS, 0);

    // read and set icon
    {
        auto iconFilename = "window-icon.xpm";

        ostringstream iconPath;

        if (getenv("APPDIR") != nullptr)
            iconPath << getenv("APPDIR") << "/";

        iconPath << std::string("resources/") + iconFilename;

        if (isFile(iconPath.str())) {
            Pixmap p, mask;
            // FIXME: remove strdup() hack which fixes error: invalid conversion from ‘const char*’ to ‘char*’
            XpmReadFileToPixmap(fl_display, DefaultRootWindow(fl_display), strdup(iconPath.str().c_str()), &p, &mask, nullptr);
            win.icon((char*) (p));
        }
    }

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

    auto showFinishedDialog = [&runApp](string msg, string newAppImagePath) {
        char* SUDO_UID = getenv("SUDO_UID");
        char* PKEXEC_UID = getenv("PKEXEC_UID");

        long originalUid = getuid();

        // check whether run from root
        if (getuid() == 0) {
            // check whether this is a re-run of AppImageUpdate with elevated permissions, in which case AppImageUpdate
            // should attempt to call the new app as the original user, and if it cannot exit with a non-zero code
            if (getenv(ELEVATED_VAR.c_str()) == nullptr) {
                if (SUDO_UID != nullptr) {
                    if ((originalUid = strtol(SUDO_UID, nullptr, 10)) == 0L) {
                        auto error = errno;
                        if (error != 0) {
                            cerr << "strtol() failed: " << strerror(error) << endl;
                            exit(1);
                        }
                    }
                }
                else if (PKEXEC_UID != nullptr) {
                    if ((originalUid = strtol(PKEXEC_UID, nullptr, 10)) == 0L) {
                        auto error = errno;
                        if (error != 0) {
                            cerr << "strtol() failed: " << strerror(error) << endl;
                            exit(1);
                        }
                    }
                }
                else {
                    fl_alert(
                        "This instance of AppImageUpdate is running with elevated permissions,\n"
                        "but cannot detect original user ID. Please run the updated AppImage yourself."
                    );
                    exit(1);
                }
            }
        }

        switch (fl_choice(msg.c_str(), "Exit", "Run application", nullptr)) {
            case 0:
                exit(0);
            case 1: {
                runApp(newAppImagePath, originalUid);
            }
        }
    };

    Updater updater(pathToAppImage);

    // first of all, check whether an update is required at all
    // this avoids unnecessary file I/O (a real update process would create a copy of the file anyway in case an
    // update is not required)
    log("Checking for updates...");
    bool updateRequired = true;

    auto updateCheckSuccessful = updater.checkForChanges(updateRequired);

    // fetch messages from updater before showing any error messages, giving the user a chance to check for errors
    {
        std::string nextMessage;
        while (updater.nextStatusMessage(nextMessage))
            log(nextMessage);
    }

    if (!updateCheckSuccessful) {
        static const string selfUpdateBinary = "appimageupdategui-selfupdate";

        // extend path to AppImage's mounting point's usr/bin/ to be able to find the binary
        ostringstream newPath;
        newPath << "PATH=" << getenv("APPDIR") << "/usr/bin:" << getenv("PATH");
        putenv(strdup(newPath.str().c_str()));

        ostringstream typeCommand;
        typeCommand << "type " << selfUpdateBinary << " 2>&1 1>/dev/null";

        // check whether in AppImage and whether self update binary is available
        auto isAppImage = (getenv("APPIMAGE") != nullptr && getenv("APPDIR") != nullptr);
        auto selfUpdateCommandAvailable = (system(typeCommand.str().c_str()) == 0);

        if (!isAppImage || !selfUpdateCommandAvailable) {
            fl_alert("Update check failed");
            exit(1);
        }

        switch (fl_choice("Update check failed.\nDo you want to look for a newer version of AppImageUpdate?",
                          "Check for updates", "Exit now", nullptr)) {
            case 0: {
                // build path
                ostringstream pathToSelfUpdateBinary;
                pathToSelfUpdateBinary << getenv("APPDIR") << "/usr/bin/" << selfUpdateBinary;

                // call selfUpdateBinary
                execl(pathToSelfUpdateBinary.str().c_str(), selfUpdateBinary.c_str(), nullptr);

                // if exec will return, it's an error
                auto err = errno;
                cerr << "Failed to call " << selfUpdateBinary << ": " << strerror(err) << endl;
                exit(2);
            }
            case 1:
                exit(1);
        }
    }

    log("... done");
    if (!updateRequired) {
        showFinishedDialog(
            "You already have the latest version.\nDo you want to run the application right now?",
            pathToAppImage
        );
        exit(0);
    }

    log("Starting update...");
    if(!updater.start()) {
        log("Failed to start update process");
        ERROR = true;
        return;
    }

    double oldProgress = 0;

    while (!updater.isDone()) {
        this_thread::sleep_for(chrono::milliseconds(100));

        double progress;
        if (!updater.progress(progress)) {
            log("Call to progress() failed");
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
        log("Update failed");
        progressBar.selection_color(FL_RED);
        progressBar.redraw();
        Fl::check();
        fl_alert("Update failed");
        exit(0);
    }

    std::string pathToUpdatedFile;

    // there is no reason for this to fail at this point, but just in case...
    if (!updater.pathToNewFile(pathToUpdatedFile))
        throw std::runtime_error("Fatal error: could not determine path to new file!");

    progressBar.selection_color(FL_GREEN);
    progressBar.redraw();
    Fl::check();
    log("Update successful.\nUpdated file: " + pathToUpdatedFile);

    string oldFile;
    auto filenameChanged = (pathToUpdatedFile == pathToAppImage);

    // check whether .zs-old file has been created, and remove it
    // if a file with a different name has been created, the file shall remain untouched on the system
    if (!filenameChanged)
        oldFile = pathToAppImage + ".zs-old";
    else
        oldFile = pathToAppImage;

    if (isFile(oldFile)) {
        // if the file has a .zs-old suffix, remove the file, otherwise leave it alone
        if (!filenameChanged)
            unlink(oldFile.c_str());
    }

    showFinishedDialog("Update successful.\nDo you want to run the application right now?", pathToUpdatedFile);

    // trigger exit to avoid FLTK warnings
    exit(0);
}

int main(const int argc, const char* const* argv) {
    cerr << "AppImageUpdate version " << APPIMAGEUPDATE_VERSION << " (commit " << APPIMAGEUPDATE_GIT_COMMIT << "), "
         << "build " << BUILD_NUMBER << " built on " << BUILD_DATE << endl;

    Fl_File_Icon::load_system_icons();

    args::ArgumentParser parser("AppImageUpdate -- GUI for updating AppImages");

    args::HelpFlag help(parser, "help", "", {'h', "help"});
    args::Flag checkForUpdate(parser, "",
        "Check for update. Exits with code 1 if changes are available, 0 if there are not,"
            "other non-zero code in case of errors.",
        {'j', "check-for-update"}
    );

    args::Positional<string> pathArg(parser, "", "Path to AppImage that should be updated");

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

    std::string pathToAppImage;

    // check whether path to AppImage has been passed on the CLI, otherwise show file chooser
    if (pathArg) {
        pathToAppImage = pathArg.Get();
    } else {
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
    }

    if (!isFile(pathToAppImage)) {
        cerr << "Cannot access file: " << pathToAppImage << endl;
        return 1;
    }

    if (checkForUpdate) {
        appimage::update::Updater updater(pathToAppImage);

        bool changesAvailable = false;

        if (!updater.checkForChanges(changesAvailable)) {
            // print all messages that might be available
            {
                std::string nextMessage;
                while (updater.nextStatusMessage(nextMessage))
                    cerr << nextMessage << endl;
            }

            cerr << "Error checking for changes!";
            return 2;
        }

        return changesAvailable ? 1 : 0;
    }

    /*
     * check whether higher privileges are necessary to be able to perform update
     * therefore, we first check whether we have write permissions to the target directory (to be able to create new
     * files, such as the temp files and the updated file), then check for write permissions to the old file (might
     * have to be able to rename it)
     * this check should be performed for both the current user and for the root user (AppImageUpdate should not
     * ignore files being read-only)
     * if root permissions could most likely help, re-execute AppImageUpdate using pkexec, gksudo, gksu (if available)
     * or show a message to the user that we can't write to the target directory, asking them to change its
     * permissions or re-execute AppImageUpdate with sudo
     */
    std::string fullPathToAppImage;

    // resolve full path to AppImage
    {
        char* pathBuf;

        if ((pathBuf = realpath(pathToAppImage.c_str(), nullptr)) == nullptr) {
            auto error = errno;
            cerr << "Failed to call readlink(): " << strerror(error) << endl;
            return 1;
        }

        fullPathToAppImage = pathBuf;
        free(pathBuf);
        pathBuf = nullptr;
    }


    // split string and get directory path
    size_t posOfLastSlash;
    if ((posOfLastSlash = fullPathToAppImage.find_last_of('/')) == std::string::npos) {
        // just making sure to catch unexpected errors
        return 1;
    }

    auto pathToAppImageDirectory = fullPathToAppImage.substr(0, posOfLastSlash);

    bool needRoot = false;

    // check whether AppImage and its directory are writable for current user
    bool appImageWritable = false;
    bool directoryWritable = false;

    if (!isFileOrDirectoryWritable(pathToAppImage, appImageWritable))
        return 1;

    if (!isFileOrDirectoryWritable(pathToAppImageDirectory, directoryWritable))
        return 1;

    if (!appImageWritable && getuid() != 0) {
        bool appImageRootWritable = false;
        bool directoryRootWritable = false;

        // normal user can't write to file and/or directory, hence retry with root
        if (!isFileOrDirectoryWritable(pathToAppImage, appImageRootWritable, true))
            return 1;

        if (!isFileOrDirectoryWritable(pathToAppImageDirectory, directoryRootWritable, true))
            return 1;

        needRoot = appImageRootWritable && directoryRootWritable;
    }


    // evaluate results
    if (!appImageWritable && getuid() == 0) {
        ostringstream oss;
        oss << "Fatal error: no write access to file " << pathToAppImage << ".\n"
            << "Please make sure you have write access to the file and retry.";
        fl_alert(oss.str().c_str());
        return 1;
    }

    if (!directoryWritable && getuid() == 0) {
        ostringstream oss;
        oss << "Fatal error: cannot write to directory " << pathToAppImageDirectory << ".\n"
            << "Please make sure you have write access to the directory and retry.";
        fl_alert(oss.str().c_str());
        return 1;
    }

    if (needRoot) {
        auto rv = fl_choice(
            "Warning: need to elevate privileges to be able to update AppImage.\n"
            "Do you want to restart the application as root?",
            "Cancel update", "Restart and retry", nullptr
        );

        switch (rv) {
            case 0: {
                return 1;
            }
            case 1: {
                std::string sudoTool = "/usr/bin/gksudo";

                // TODO: check whether sudoTool exists
                // TODO: also check whether pkexec exists (more modern UX)
                // TODO: find out why pkexec won't work if exec()d (something about suid bit not being set although it is set)

                // prepare argv array for exec() call
                vector<char*> newArgv;

                newArgv.push_back(strdup(sudoTool.c_str()));
                // trying to use STL functions like std::copy didn't lead to success within a reasonable amount of time
                // hence using a simple for loop
                for (int i = 0; i < argc; i++) {
                    newArgv.push_back(strdup(argv[i]));
                }
                // null-terminate the vector to make execvpe happy
                newArgv.push_back(nullptr);

                // push new variable into environ stating that AppImageUpdate had to elevate to root privileges
                // then, the new instance knows it should try to downgrade to the old user ID when the user wants to
                // run the app, and if it can't, abort running the AppImage
                // also allows it to recognize whether AppImageUpdate has been started with root permissions directly
                // in which case running the application as root after the update is valid
                putenv(strdup((ELEVATED_VAR + std::string("=1")).c_str()));

                execvp(sudoTool.c_str(), newArgv.data());

                // exec would only return in case of error
                auto error = errno;
                cerr << "Failed to call execv(): " << strerror(error) << endl;
                return 1;
            }
            default:
                return 3;
        }
    }

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
