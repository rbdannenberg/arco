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

Bridge_info *arco_bridge = NULL;


void arco4_initialize()
{
    int rslt = o2_shmem_initialize();
    assert(rslt == O2_SUCCESS);
    arco_bridge = o2_shmem_inst_new();
    audioio_initialize(arco_bridge);
}


