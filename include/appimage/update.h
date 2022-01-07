#pragma once

// global headers
#include <sys/types.h>
#include <string>

namespace appimage::update {
    /**
     * Primary class of AppImageUpdate. Abstracts entire functionality.
     *
     * Update is run asynchronously in a separate thread. The owner of the instance can query the progress.
     */
    class Updater {
    public:
        // Represents current state of the updater
        enum State {
            INITIALIZED,
            RUNNING,
            STOPPING,
            SUCCESS,
            ERROR,
        };

        // Validation states. Returned by validate()
        enum ValidationState {
            // there is only one PASSED state, hence check like == PASSED
            VALIDATION_PASSED = 0,

            // warning states -- check like >= WARNING && < ERROR
            VALIDATION_WARNING = 1000,
            VALIDATION_NOT_SIGNED,
            VALIDATION_GPG_MISSING,

            // error states -- check like >= ERROR
            VALIDATION_FAILED = 2000,
            VALIDATION_KEY_CHANGED,
            VALIDATION_GPG_CALL_FAILED,
            VALIDATION_TEMPDIR_CREATION_FAILED,
            VALIDATION_NO_LONGER_SIGNED,
            VALIDATION_BAD_SIGNATURE,
        };

    private:
        // opaque private class
        // without this pattern, the header would require C++11, which is undesirable
        class Private;
        Private* d;

    private:
        // thread runner -- should be called from start() only
        void runUpdate();

    public:
        // throws std::invalid_argument if the file does not exist
        // if overwrite is specified, old file will be overwritten, otherwise it will remain on the system
        // as-is
        explicit Updater(const std::string& pathToAppImage, bool overwrite = false);
        ~Updater();

    public:
        // Start update process. If running/finished already, returns false, otherwise true.
        bool start();

        // Interrupt update process as soon as possible. Throws exception if the update has not been started.
        // Returns false if stop() has been called already.
        bool stop();

        // Returns current state of the updater.
        State state();

        // Convenience function returning true when the update has finished. Uses state() internally.
        // Beware that it will return true in case of errors, too! Combine with either state() or hasError()!
        bool isDone();

        // Convenience function returning whether an error has occured. Uses state() internally.
        // Beware that it will return false even if the update process has not yet begun, or is currently running!
        bool hasError();

        // Sets given parameter to current progress. Returns false in case of failure, i.e., the update process
        // is not running or the version of the AppImage format is not supported, otherwise true.
        bool progress(double& progress);

        // Fetch a status message from the client in use that can be used to display updates
        bool nextStatusMessage(std::string& message);

        // Check whether an update is available
        // Please note that this method is *only* available until the update is started (after calling start(),
        // the method will instantly return false)
        bool checkForChanges(bool& updateAvailable, unsigned int method = 0);

        // Parses AppImage file, and returns a formatted string describing it
        // in case of success, sets description and returns true, false otherwise
        bool describeAppImage(std::string& description) const;

        // Sets path to the path of the file created by the update and returns true as soon as this value is
        // available (after a successful update at the latest)
        // Returns false in case of errors, or when the path is not available yet
        bool pathToNewFile(std::string& path) const;

        // Validate AppImage signature
        // TODO: describe process
        // Returns a ValidationState value. See ValidationState documentation for more information.
        ValidationState validateSignature();

        // Returns a description string of the given validation state.
        static std::string signatureValidationMessage(const ValidationState& state);

        // Returns the size of the remote file in bytes
        bool remoteFileSize(off_t& fileSize) const;

        // Return update information
        // If no custom update information is set, the function returns the raw update information contained in the
        // AppImage
        [[nodiscard]] std::string updateInformation() const;

        // Set custom update information
        // The custom update information will overwrite the one fetched from the AppImage
        // This function is for applications that use libappimageupdate, e.g., to self-update, or implement a custom
        // frontend like AppImageLauncher does, extending those users' possibilities
        // Use cases include the use of custom update servers instead of relying on a built-in update information type,
        // implement multiple update channels, etc.
        void setUpdateInformation(std::string newUpdateInformation);

        // Restore original file, e.g., after a signature validation error
        bool restoreOriginalFile();

        // copy permissions of the original AppImage to the new version
        void copyPermissionsToNewFile();
    };
}
