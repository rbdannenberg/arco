/* fileiothread.cpp -- abstract thread to hide pthread and CreateThread
 *
 * Roger B. Dannenberg
 * July 2024
 *
 * Used by fileio.h and fileio.cpp
 */

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN 1
#include "windows.h"
#else
#include "pthread.h"
#include "sys/select.h"
#endif

#include "assert.h"
#include "fileiothread.h"

// Currently, we allow only one instance of Fileio_thread so that
// we can hide details here in fileiothread.cpp (otherwise, they
// would be members of Fileio_thread, so client would see header
// for pthread or Windows threads.

#ifdef WIN32
static HANDLE fileio_thread_id;
#else
static pthread_t fileio_thread_id;
#endif

static bool fileio_thread_created = false;

// redirect thread to Fileio_thread::thread_main()
#ifdef WIN32
DWORD WINAPI fileio_thread_run(LPVOID data)
{
    ((Fileio_thread *) data)->thread_main();
    return 0;
}

#else
void *fileio_thread_run(void *data)
{
    ((Fileio_thread *) data)->thread_main();
    return 0;
}
#endif

// returns 0 on success creating thread. Otherwise, no thread is created
// and caller should clean up.
int Fileio_thread::initialize(int per)
{
    assert(!fileio_thread_created);
    fileio_thread_created = true;  // no more threads after this!
#ifdef WIN32
    fileio_thread_id = CreateThread( 
            NULL,                   // default security attributes
            0,                      // use default stack size  
            fileio_thread_run,      // thread function name
            this,                   // argument to thread function 
            0,                      // use default creation flags 
            0);                     // don't request thread identifier 

    return fileio_thread_id == NULL;
#else
    return pthread_create(&fileio_thread_id, NULL, fileio_thread_run, this);
#endif
}


void Fileio_thread::thread_main()
{
    thread_started();
    while (!quit_request) {
        poll();  // will call o2sm_poll()
#ifdef WIN32
        Sleep(period);
#else
        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = period * 1000;
        select(0, NULL, NULL, NULL, &timeout);
#endif    
    }
    // when quit_request is true, close up
    cleanup();

    // Delete the thread
#ifdef WIN32
    fileio_thread_id = NULL;
    // thread is cleaned up when we return from this function
#else
#endif
    fileio_thread_created = false;
}
