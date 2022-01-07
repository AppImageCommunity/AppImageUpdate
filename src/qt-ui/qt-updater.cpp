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
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QProgressDialog>
#include <QTimer>

// local headers
#include "appimage/update/qt-ui.h"
#include "appimage/update.h"
#include "spoiler.h"
#include "util.h"


namespace appimage {
    namespace update {
        using namespace util;

        namespace qt {
            class QtUpdater::Private {
            public:
                const QString pathToAppImage;
                appimage::update::Updater* updater;

                QLabel* label;
                QHBoxLayout* labelLayout;
                QLabel* progressLabel;
                QLabel* validationStateLabel;
                QDialogButtonBox* buttonBox;

                QProgressBar* progressBar;

                QLayout* mainLayout;

                QString appName;
                QString appImageFileName;

                QTimer* progressTimer;

                Spoiler* spoiler;
                QVBoxLayout* spoilerLayout;
                QPlainTextEdit* spoilerLog;

                bool finished;

                const int minimumWidth;

                bool enableRunUpdatedAppImageButton;

            public:
                explicit Private(const QString& pathToAppImage) : buttonBox(nullptr),
                                                                  progressBar(nullptr),
                                                                  mainLayout(nullptr),
                                                                  label(nullptr),
                                                                  progressTimer(nullptr),
                                                                  progressLabel(nullptr),
                                                                  pathToAppImage(pathToAppImage),
                                                                  spoiler(nullptr),
                                                                  spoilerLayout(nullptr),
                                                                  spoilerLog(nullptr),
                                                                  finished(false),
                                                                  minimumWidth(400),
                                                                  enableRunUpdatedAppImageButton(false)
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
                    delete spoiler;
                }

            public:
                void startUpdate() {
                    updater->start();
                }

                void printStatusMessages(const QtUpdater* self, Updater& updater) {
                    std::string nextMessage;

                    while (updater.nextStatusMessage(nextMessage)) {
                        emit self->newStatusMessage(nextMessage);
                    }
                }
            };

            QtUpdater::QtUpdater(const QString& pathToAppImage) {
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

                d->mainLayout = new QVBoxLayout();
                setLayout(d->mainLayout);

                // make sure the QDialog resizes with the spoiler
                layout()->setSizeConstraint(QLayout::SetFixedSize);

                d->label = new QLabel(QString("Updating " + d->appImageFileName + "..."));
                d->label->setMinimumWidth(d->minimumWidth);
                layout()->addWidget(d->label);

                d->progressBar = new QProgressBar();
                d->progressBar->setMinimumWidth(d->minimumWidth);
                d->progressBar->setMinimum(0);
                d->progressBar->setMaximum(100);
                layout()->addWidget(d->progressBar);

                d->progressLabel = new QLabel(this);
                d->progressLabel->setMinimumWidth(d->minimumWidth);
                d->progressLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
                d->progressLabel->setText("Starting update...");
                layout()->addWidget(d->progressLabel);

                d->spoiler = new Spoiler("Details");
                d->spoiler->resize(QSize(d->minimumWidth, 200));
                d->spoilerLayout = new QVBoxLayout();
                d->spoilerLog = new QPlainTextEdit();
                d->spoilerLog->setReadOnly(true);
                d->spoilerLayout->addWidget(d->spoilerLog);
                d->spoiler->setContentLayout(*d->spoilerLayout);
                layout()->addWidget(d->spoiler);

                d->buttonBox = new QDialogButtonBox(QDialogButtonBox::Cancel);
                connect(d->buttonBox, SIGNAL(rejected()), this, SLOT(showCancelDialog()));
                layout()->addWidget(d->buttonBox);

                d->progressTimer = new QTimer(this);
                connect(d->progressTimer, SIGNAL(timeout()), this, SLOT(updateProgress()));
                d->progressTimer->start(100);

                adjustSize();

                // default run action
                connect(this, SIGNAL(runUpdatedAppImageClicked()), this, SLOT(runUpdatedAppImage()));

                // connect log method
                connect(this, SIGNAL(newStatusMessage(const std::string&)), this, SLOT(processNewStatusMessage(const std::string&)));
            }

            void QtUpdater::processNewStatusMessage(const std::string& nextMessage) {
                // print message to stderr
                std::cerr << nextMessage << std::endl;

                // if spoilerLog is available, also print message there
                if (d->spoilerLog != nullptr) {
                    d->spoilerLog->moveCursor(QTextCursor::End);
                    std::ostringstream oss;
                    oss << nextMessage << std::endl;
                    d->spoilerLog->insertPlainText(QString::fromStdString(oss.str()));
                }
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

                d->printStatusMessages(this, *d->updater);

                if (d->updater->isDone()) {
                    d->finished = true;

                    d->progressTimer->stop();

                    auto palette = d->progressBar->palette();

                    // can only validate signature once the update has finished, otherwiws
                    Updater::ValidationState validationResult = Updater::VALIDATION_FAILED;
                    QString validationMessage;

                    // TODO: refactor this block, it's quite difficult to understand right now
                    if (d->updater->hasError()) {
                        d->label->setText("Update failed!");
                        palette.setColor(QPalette::Highlight, Qt::red);
                    } else {
                        // can
                        validationResult = d->updater->validateSignature();
                        validationMessage = QString::fromStdString(d->updater->signatureValidationMessage(validationResult));

                        if (validationResult != d->updater->VALIDATION_PASSED) {
                            if (validationResult >= d->updater->VALIDATION_WARNING && validationResult < d->updater->VALIDATION_FAILED) {
                                if (validationResult == d->updater->VALIDATION_NOT_SIGNED) {
                                    // copy permissions of the old AppImage to the new version
                                    d->updater->copyPermissionsToNewFile();
                                }

                                d->label->setText("Signature validation problem: " + validationMessage);
                                palette.setColor(QPalette::Highlight, Qt::yellow);
                                palette.setColor(QPalette::HighlightedText, Qt::black);
                            } else {
                                d->updater->restoreOriginalFile();
                                const QString message = "Signature validation error: " + validationMessage;
                                d->label->setText(message);
                                palette.setColor(QPalette::Highlight, Qt::red);
                                QMessageBox::critical(this, "Error", message + "\n\nRestoring original file");
                            }
                        } else {
                            // copy permissions of the old AppImage to the new version
                            d->updater->copyPermissionsToNewFile();

                            if (validationResult == d->updater->VALIDATION_PASSED)
                                newStatusMessage("Signature validation passed");
                            d->label->setText("Update successful!");
                            palette.setColor(QPalette::Highlight, Qt::green);
                            palette.setColor(QPalette::HighlightedText, Qt::black);
                        }
                    }

                    // TODO: doesn't work with the Gtk+ platform theme
                    d->progressBar->setPalette(palette);

                    // replace button box
                    disconnect(d->buttonBox, SIGNAL(rejected()));
                    delete d->buttonBox;

                    d->buttonBox = new QDialogButtonBox();

                    if (!d->updater->hasError() && validationResult < d->updater->VALIDATION_FAILED && d->enableRunUpdatedAppImageButton) {
                        d->buttonBox->addButton("Run updated AppImage", QDialogButtonBox::AcceptRole);
                        connect(d->buttonBox, &QDialogButtonBox::accepted, this, [this](){
                            emit runUpdatedAppImageClicked();
                        });
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

                try {
                    if (updater.updateInformation().empty())
                        return -1;
                } catch (const std::runtime_error&) {
                    return -1;
                }

                bool changesAvailable = false;

                auto result = updater.checkForChanges(changesAvailable);

                // print all messages that might be available
                d->printStatusMessages(this, updater);

                if (!result)
                    return 2;

                if (changesAvailable) {
                    if (writeToStderr)
                        std::cerr << "Update available" << std::endl;
                    return 1;
                } else {
                    if (writeToStderr)
                        std::cerr << "AppImage already up to date" << std::endl;
                    return 0;
                }
            }

            void QtUpdater::closeEvent(QCloseEvent* event) {
                if (!d->finished) {
                    // ignore event...
                    event->ignore();
                    // ... and show cancel dialog
                    showCancelDialog();
                }
            }

            void QtUpdater::showEvent(QShowEvent* event) {
                QDialog::showEvent(event);

                d->startUpdate();
            }

            bool QtUpdater::pathToNewFile(QString& pathToNewAppImage) const {
                std::string stdPathToNewAppImage;

                if (!d->updater->pathToNewFile(stdPathToNewAppImage))
                    return false;

                pathToNewAppImage = QString::fromStdString(stdPathToNewAppImage);
                return true;
            }

            void QtUpdater::runUpdatedAppImage() {
                QString pathToNewAppImage;

                if (!pathToNewFile(pathToNewAppImage))
                    throw std::runtime_error("Could not detect path to new AppImage");

                runApp(pathToNewAppImage.toStdString());
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

            void QtUpdater::enableRunUpdatedAppImageButton(bool enable) {
                d->enableRunUpdatedAppImageButton = enable;
            }
        }
    }
}
