// system headers
#include <chrono>
#include <iterator>
#include <iostream>
#include <thread>

// library headers
#include <QCommandLineParser>
#include <QFileDialog>
#include <QApplication>
#include <zsutil.h>

// local headers
#include "appimage/update.h"
#include "appimage/update/qt-ui.h"
#include "../util.h"

using namespace std;
using namespace appimage::update;

int main(int argc, char** argv) {
    cerr << "AppImageUpdate-Qt version " << APPIMAGEUPDATE_VERSION << " (commit " << APPIMAGEUPDATE_GIT_COMMIT << "), "
         << "build " << BUILD_NUMBER << " built on " << BUILD_DATE << endl;

    QCommandLineParser parser;

    parser.addHelpOption();
//    parser.setSingleDashWordOptionMode(QCommandLineParser::ParseAsLongOptions);

    parser.setApplicationDescription(QObject::tr(
        "AppImageUpdate -- GUI for updating AppImages, Qt edition"
    ));

    QCommandLineOption showVersion({"v", "version"}, "Display version and exit.");
    parser.addOption(showVersion);

    QCommandLineOption checkForUpdate(
        {"j", "check-for-update"},
        "Check for update. Exits with code 1 if changes are available, 0 if there are not,"
            "other non-zero code in case of errors."
    );
    parser.addOption(checkForUpdate);

    QCommandLineOption selfUpdate("self-update", "Update the tool itself and exit.");
    parser.addOption(selfUpdate);

    parser.addPositionalArgument("path", "Path to AppImage that should be updated", "<AppImage>");

    QStringList arguments;
    for (int i = 0; i < argc; i++)
        arguments.push_back(argv[i]);

    parser.parse(arguments);

    if (parser.isSet(showVersion)) {
        // version has been printed already, so we can just exit here
        return 0;
    }

    QApplication app(argc, argv);

    QString pathToAppImage;

    appimage::update::qt::QtUpdater* updater;

    // if a self-update is requested, check whether the path argument has been passed, and show an error
    // otherwise check whether path has been passed on the CLI, otherwise show file chooser
    if (parser.isSet(selfUpdate)) {
        if (!parser.positionalArguments().empty()) {
            cerr << "Error: --self-update does not take a path." << endl;
            parser.showHelp(1);
        } else {
            updater = appimage::update::qt::QtUpdater::fromEnv();

            if (updater == nullptr) {
                cerr << "Error: self update requested but could not determine path to AppImage "
                     << "($APPIMAGE environment variable missing)."
                     << endl;
                return 1;
            }
        }
    } else {
        if (!parser.positionalArguments().empty()) {
            pathToAppImage = parser.positionalArguments().front();
        } else {
            pathToAppImage = QFileDialog::getOpenFileName(
                Q_NULLPTR,
                "Please choose an AppImage for updating",
                QDir::currentPath(),
                "AppImage (*.appimage *.AppImage);;All files (*)"
            );
            if (pathToAppImage.isNull()) {
                cerr << "No file selected, exiting.";
                return 1;
            }
        }

        // make absolute
        pathToAppImage = QFileInfo(pathToAppImage).absoluteFilePath();

        updater = new appimage::update::qt::QtUpdater(pathToAppImage);
    }

    if (parser.isSet(checkForUpdate))
        return updater->checkForUpdates(true);

    updater->show();

    auto rv = app.exec();

    delete updater;
    return rv;
}
