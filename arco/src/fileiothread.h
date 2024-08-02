/* fileiothread.h -- abstract thread to hide pthread and CreateThread
 *
 * Roger B. Dannenberg
 * July 2024
 *
 * Used by fileio.h and fileio.cpp
 *
 * A Fileio_thread is an abstract class intended to provide a thread
 * that runs in shared memory and communicates via O2 shared memory API.
 * This is the case for asynchronous file IO in Arco.
 *
 * There is no real reason for this to be an abstract class, but it is
 * stuctured this way to isolate the thread management, which could either
 * build on pthreads (Linux, MacOS) or CreateThread (Windows).
 *
 */


class Fileio_thread {
  public:
    int period;  // polling period in ms
    bool quit_request;

    // to start the thread, use initialize():
    int initialize(int per);

    // the entry point for the new thread:
    void thread_main();

    // thread_started() will be called once when thread starts
    virtual void thread_started() = 0;

    // poll() will be called every period ms:
    virtual void poll() = 0;

    // to stop the thread and clean up, call quit:
    void quit() { quit_request = true; }

    // Upon stopping the thread, cleanup() is called
    // to allow the subclass to take action:
    virtual void cleanup() = 0;
};
