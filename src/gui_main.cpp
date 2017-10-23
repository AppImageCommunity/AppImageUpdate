// system headers
#include <chrono>
#include <iterator>
#include <iostream>
#include <thread>

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
static bool DONE = false;

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

// to be run in a thread
void runUpdate(
    const std::string pathToAppImage,
    Fl_Window* win,
    Fl_Progress* progressBar,
    Fl_Text_Display* textDisplay,
    Fl_Text_Buffer* textBuffer
) {
    auto log = [&textDisplay, &textBuffer](const std::string& msg) {
        std::ostringstream message;
        message << msg << endl;

        cout << message.str();

        textBuffer->insert(textBuffer->length() + 1, message.str().c_str());
        textDisplay->scroll(INT_MAX, 0);

        Fl::check();
    };

    Updater updater(pathToAppImage);

    log("Starting update...");
    if(!updater.start()) {
        log("Failed to start update process!");
        DONE = true;
        return;
    }

    double oldProgress = 0;

    while (!updater.isDone()) {
        this_thread::sleep_for(chrono::milliseconds(100));

        double progress;
        if (!updater.progress(progress)) {
            log("Call to progress() failed!");
            DONE = true;
            return;
        }

        progress *= 100;

        // check for change to avoid having to redraw every 100ms
        if (progress != oldProgress) {
            progressBar->value(static_cast<float>(progress));

            ostringstream label;
            label << progress << "%";
            progressBar->label(label.str().c_str());

            // update UI
            Fl::check();
        }

        std::string nextMessage;
        while (updater.nextStatusMessage(nextMessage))
            log(nextMessage);
    }

    if (updater.hasError()) {
        log("Update failed!");
    } else {
        log("Successfully updated AppImage!");
    }

    DONE = true;
}

void windowCallback(Fl_Widget *widget, void *) {
    if (DONE) {
        exit(0);
    }
}

int main(const int argc, const char* const* argv) {
    // check whether path to AppImage has been passed on the CLI, otherwise show file chooser
    std::string pathToAppImage;
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

    IDesktopEnvironment* desktopEnvironment = IDesktopEnvironment::getInstance();

    std::string font;

    if (desktopEnvironment != nullptr
        && desktopEnvironment->gtkInterfaceFont(font)) {
        setFltkFont(font);
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

    // run worker thread so UI can run in main thread
    thread workerThread(runUpdate,
        pathToAppImage,
        &win,
        &progressBar,
        &textDisplay,
        &textBuffer
    );

    auto result = Fl::run();

    // wait for worker thread
    workerThread.join();

    delete desktopEnvironment;
    return result;
}
