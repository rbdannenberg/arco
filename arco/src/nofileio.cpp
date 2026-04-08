/* nofileio.cpp -- stub replacement for file io that does nothing
 *
 * Roger B. Dannenberg
 * April 2023
 *
 * See fileio.h for description.
 */

#include <stdio.h>
#include <fcntl.h>
#ifndef WIN32
#include <unistd.h>
#endif
#include "nofileio.h"

bool fileio_finished = false;
    
// to be called from main thread.
//
int fileio_initialize()
{
    fileio_finished = true;
    return 0;
}
