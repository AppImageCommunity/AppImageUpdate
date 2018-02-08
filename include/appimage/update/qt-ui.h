#include <QDialog>

namespace appimage {
    namespace update {
        namespace qt {
            class QtUpdater : public QDialog {
                Q_OBJECT

            private:
                class Private;
                Private* d;

            public:
                explicit QtUpdater(QString& pathToAppImage);
                ~QtUpdater() override;

            Q_SIGNALS:
                void canceled();

            private Q_SLOTS:
                void updateProgress();
                void runUpdatedAppImage();
                void showCancelDialog();
                void cancelUpdate();

            private:
                void init();

            protected:
                void closeEvent(QCloseEvent* event) override;
                void showEvent(QShowEvent* event) override;
                void keyPressEvent(QKeyEvent* event) override;

            public:
                // check for updates
                // returns 0 if there's no update, 1 if there's an update available, any other value indicates an error
                // if writeToStderr is set, writes status messages to stderr, which can be used for debugging etc.
                int checkForUpdates(bool writeToStderr = false) const;

                // create new QtUpdater instance from environment variable (i.e., when being called from within an
                // AppImage, this method configures the instance automatically)
                // returns nullptr if application isn't run from within an AppImage
                static QtUpdater* fromEnv();
            };
        }
    }
}
