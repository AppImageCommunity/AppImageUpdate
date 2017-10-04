// system headers
#include <chrono>
#include <fstream>
#include <mutex>
#include <random>
#include <thread>

// local headers
#include "appimage/update.h"

// convenience declaration
typedef std::lock_guard<std::mutex> lock_guard;

namespace appimage {
    namespace update {
        class Updater::PrivateData {
        public:
            PrivateData() : state(INITIALIZED),
                            pathToAppImage(),
                            progress(0),
                            thread(nullptr),
                            mutex() {};

        public:
            // data
            std::string pathToAppImage;

            // state
            State state;
            double progress;

            // threading
            std::thread* thread;
            std::mutex mutex;
        };
        
        Updater::Updater(std::string pathToAppImage) {
            // initialize data class
            d = new Updater::PrivateData();

            // check whether file exists, otherwise throw exception
            std::ifstream f(pathToAppImage);

            if(!f.good())
                throw std::invalid_argument("No such file or directory: " + d->pathToAppImage);

            d->pathToAppImage = pathToAppImage;
        }

        void Updater::runUpdate() {
            // initialization
            {
                lock_guard guard(d->mutex);

                // make sure it runs only once at a time
                // should never occur, but you never know
                if (d->state != INITIALIZED)
                    return;

                d->state = RUNNING;
            }

            // create a PRNG
            std::random_device rd;
            std::default_random_engine engine(rd());

            // run phase
            {
                // sleep for some time to simulate update process
                std::uniform_int_distribution<> distribution(1000, 15000);

                // time that should be slept
                const auto timeTotal = distribution(engine);

                auto timeRun = 0;

                while(timeRun < timeTotal) {
                    // amount of microseconds to sleep now
                    static const auto iterationSleepTime = 100;

                    std::this_thread::sleep_for(std::chrono::milliseconds(iterationSleepTime));
                    timeRun += iterationSleepTime;

                    // now, lock mutex to update the class variables
                    lock_guard guard(d->mutex);

                    d->progress = (double) timeRun / (double) timeTotal;

                    // normalize as value could exceed 1
                    if(d->progress > 1)
                        d->progress = 1;
                }
            }

            // end phase
            {
                lock_guard guard(d->mutex);

                // fail update at chance of 15%
                std::uniform_int_distribution<> distribution(0, 100);

                auto value = distribution(engine);

                if(value < 15)
                    d->state = ERROR;
                else
                    d->state = SUCCESS;
            }
        }

        bool Updater::start() {
            // lock mutex
            lock_guard guard(d->mutex);

            // prevent multiple start calls
            if(d->state != INITIALIZED)
                return false;

            // if there's a thread managed by this class already, should not start another one and lose access to
            // this one
            if(d->thread)
                return false;

            // create thread
            d->thread = new std::thread(&Updater::runUpdate, this);

            return true;
        }

        bool Updater::isDone() {
            return d->state != INITIALIZED && d->state != RUNNING && d->state != STOPPING;
        }

        bool Updater::hasError() {
            return d->state == ERROR;
        }

        double Updater::progress() {
            return d->progress;
        }

        bool Updater::stop() {
            throw std::runtime_error("not implemented");
        }
    }
}
