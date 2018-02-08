// global headers
#include <iostream>

// library headers
#include <QCloseEvent>
#include <QDialogButtonBox>
#include <QFileInfo>
#include <QGroupBox>
#include <QLabel>
#include <QLayout>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QProgressDialog>
#include <QTimer>

// local headers
#include "appimage/update/qt-ui.h"
#include "appimage/update.h"
#include "util.h"

namespace appimage {
    namespace update {
        namespace qt {
            class QtUpdater::Private {
            public:
                const QString pathToAppImage;
                appimage::update::Updater* updater;

                QLabel* label;
                QLabel* progressLabel;
                QDialogButtonBox* buttonBox;

                QProgressBar* progressBar;

                QLayout* mainLayout;

                QString appName;
                QString appImageFileName;

                QTimer* progressTimer;

            public:
                explicit Private(QString& pathToAppImage) : buttonBox(nullptr),
                                                            progressBar(nullptr),
                                                            mainLayout(nullptr),
                                                            label(nullptr),
                                                            progressTimer(nullptr),
                                                            pathToAppImage(pathToAppImage)
                {
                    if (!isFile(pathToAppImage.toStdString()))
                        throw std::runtime_error("No such file or directory: " + pathToAppImage.toStdString());

                    updater = new Updater(pathToAppImage.toStdString());

                    auto fileInfo = QFileInfo(pathToAppImage);

                    {
                        auto appName = fileInfo.baseName();

                        QStringList archs;
                        archs << "x86_64" << "i386" << "i586" << "i686" << "x64" << "x86";
                        for (const auto& arch : archs)
                            appName.replace(arch, "");

                        auto stdAppName = appName.toStdString();
                        trim(stdAppName, '-');
                        appName = QString::fromStdString(stdAppName);

                        this->appName = appName;
                    }

                    appImageFileName = fileInfo.baseName() + "." + fileInfo.suffix();
                }

                ~Private() {
                    delete updater;
                    delete label;
                    delete progressLabel;
                    delete buttonBox;
                    delete progressBar;
                    delete mainLayout;
                    delete progressTimer;
                }

            public:
                void startUpdate() {
                    updater->start();
                }
            };

            QtUpdater::QtUpdater(QString& pathToAppImage) {
                d = new Private(pathToAppImage);

                init();
            }

            QtUpdater::~QtUpdater() {
                delete d;
            }

            void QtUpdater::init() {
                // replace default cancel button handling
                setWindowTitle(QString("Updating " + d->appName));
                // make it a modal dialog
                // doesn't have an effect in the standalone AppImageUpdate-Qt app, but should improve UX when being
                // integrated in other apps
                setModal(true);

                resize(QSize(360, 150));

                d->mainLayout = new QVBoxLayout();
                setLayout(d->mainLayout);

                d->label = new QLabel(QString("Updating " + d->appImageFileName + "..."));
                layout()->addWidget(d->label);

                d->progressBar = new QProgressBar();
                d->progressBar->resize(200, 24);
                d->progressBar->setMinimum(0);
                d->progressBar->setMaximum(100);
                layout()->addWidget(d->progressBar);

                d->progressLabel = new QLabel(this);
                layout()->addWidget(d->progressLabel);

                d->buttonBox = new QDialogButtonBox(QDialogButtonBox::Cancel);
                connect(d->buttonBox, SIGNAL(rejected()), this, SLOT(showCancelDialog()));
                layout()->addWidget(d->buttonBox);

                d->progressTimer = new QTimer(this);
                connect(d->progressTimer, SIGNAL(timeout()), this, SLOT(updateProgress()));
                d->progressTimer->start(100);
            }

            void QtUpdater::updateProgress() {
                double progress;

                if (!d->updater->progress(progress))
                    return;

                d->progressBar->setValue(int(progress * 100));

                off_t fileSize;
                if (d->updater->remoteFileSize(fileSize)) {
                    double downloadedSize = progress * fileSize;
                    std::stringstream ss;
                    ss << std::fixed << std::setprecision(1)
                       << downloadedSize / 1024.0f / 1024.0f << " MiB of " << fileSize / 1024.0f / 1024.0f << " MiB";
                    d->progressLabel->setText(QString::fromStdString(ss.str()));
                }

                std::string nextMessage;
                while (d->updater->nextStatusMessage(nextMessage))
                    std::cerr << nextMessage << std::endl;

                if (d->updater->isDone()) {
                    d->progressTimer->stop();

                    auto palette = d->progressBar->palette();

                    if (d->updater->hasError()) {
                        d->label->setText("Update failed!");
                        palette.setColor(QPalette::Highlight, Qt::red);
                    } else {
                        d->label->setText("Update successful!");
                        palette.setColor(QPalette::Highlight, Qt::green);
                    }

                    // TODO: doesn't work with the Gtk+ platform theme
                    d->progressBar->setPalette(palette);

                    // replace button box
                    disconnect(d->buttonBox, SIGNAL(rejected()));
                    delete d->buttonBox;

                    d->buttonBox = new QDialogButtonBox();

                    if (!d->updater->hasError()) {
                        d->buttonBox->addButton("Run updated AppImage", QDialogButtonBox::AcceptRole);
                        connect(d->buttonBox, SIGNAL(accepted()), this, SLOT(runUpdatedAppImage()));
                    }

                    d->buttonBox->addButton("Close", QDialogButtonBox::RejectRole);

                    connect(d->buttonBox, &QDialogButtonBox::rejected, this, [this]() {
                        this->done(0);
                    });

                    layout()->addWidget(d->buttonBox);
                }
            }

            int QtUpdater::checkForUpdates(bool writeToStderr) const {
                appimage::update::Updater updater(d->pathToAppImage.toStdString());

                bool changesAvailable = false;

                auto result = updater.checkForChanges(changesAvailable);

                // print all messages that might be available
                if (writeToStderr) {
                    std::string nextMessage;
                    while (updater.nextStatusMessage(nextMessage))
                        std::cerr << nextMessage << std::endl;
                }

                if (!result)
                    return 2;

                if (changesAvailable) {
                    std::cerr << "Update available" << std::endl;
                    return 1;
                } else {
                    std::cerr << "AppImage already up to date" << std::endl;
                    return 0;
                }
            }

            void QtUpdater::closeEvent(QCloseEvent* event) {
                // ignore event...
                event->ignore();

                // ... and show cancel dialog
                showCancelDialog();
            }

            void QtUpdater::showEvent(QShowEvent* event) {
                QDialog::showEvent(event);

                d->startUpdate();
            }

            void QtUpdater::runUpdatedAppImage() {
                std::string pathToNewAppImage;

                if (!d->updater->pathToNewFile(pathToNewAppImage))
                    throw std::runtime_error("Could not detect path to new AppImage");

                runApp(pathToNewAppImage);
                done(0);
            }

            void QtUpdater::showCancelDialog() {
                // prepare message box
                auto button = QMessageBox::critical(
                    this,
                    "Cancel update", "Do you want to cancel the update process?",
                    QMessageBox::No | QMessageBox::Yes,
                    QMessageBox::Yes
                );

                switch (button) {
                    case QMessageBox::Yes:
                        cancelUpdate();
                        break;
                    default:
                        break;
                }
            }

            void QtUpdater::cancelUpdate() {
                std::cerr << "canceled" << std::endl;

                if (!d->updater->isDone())
                    d->updater->stop();

                done(1);
            }

            QtUpdater* QtUpdater::fromEnv() {
                auto* APPIMAGE = getenv("APPIMAGE");

                if (APPIMAGE == nullptr || !isFile(APPIMAGE))
                    return nullptr;

                auto pathToAppImage = QString(APPIMAGE);
                return new QtUpdater(pathToAppImage);
            }

            void QtUpdater::keyPressEvent(QKeyEvent* event) {
                if (event->key() == Qt::Key_Escape) {
                    event->ignore();
                    showCancelDialog();
                } else {
                    QDialog::keyPressEvent(event);
                }
            }
        }
    }
}
