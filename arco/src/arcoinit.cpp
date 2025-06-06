/* arco_init -- initialize the arco server as a shared memory thread
 *
 * Roger B. Dannenberg
 * Feb 2022
 */

#include <stdio.h>
#include <fcntl.h>
#ifndef WIN32
#include <unistd.h>
#endif
#include "o2internal.h"  // need internal to offer bridge
#include "o2atomic.h"
#include "sharedmem.h"   // o2_shmem_inst_new()
#include "arcotypes.h"
#include "audioio.h"
#include "fileio.h"


int arco_initialize()
{
    return audioio_initialize() ||
           fileio_initialize();
}
