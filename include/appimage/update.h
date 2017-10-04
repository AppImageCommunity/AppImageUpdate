// global headers
#include <string>

namespace appimage {
    namespace update {
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

        private:
            // opaque data type
            // without this pattern, the header would become C++11, which is undesirable
            class PrivateData;
            PrivateData* d;

        private:
            // thread runner -- should be called from start() only
            void runUpdate();

        public:
            Updater(std::string pathToAppImage);

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

            // Returns a value between 0 and 1 indicating update progress.
            double progress();
        };
    }
}
