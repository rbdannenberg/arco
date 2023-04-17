/* arco_init -- initialize the arco server as a shared memory thread
 *
 * Roger B. Dannenberg
 * Feb 2022
 */

#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include "o2internal.h"  // need internal to offer bridge
#include "sharedmem.h"   // o2_shmem_inst_new()
#include "arcotypes.h"
#include "audioio.h"
#include "fileio.h"

Bridge_info *audio_bridge = NULL;


void arco_initialize()
{
    audioio_initialize();
    fileio_initialize();
}
