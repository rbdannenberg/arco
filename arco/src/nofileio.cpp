/* nofileio.cpp -- stub replacement for file io that does nothing
 *
 * Roger B. Dannenberg
 * April 2023
 *
 * See fileio.h for description.
 */

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include "nofileio.h"

    
// to be called from main thread.
//
int fileio_initialize()
{
    return 0;
}
