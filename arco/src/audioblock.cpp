/* audioblock.cpp - audio blocks are shared memory use for audio file IO
 *
 * Roger B. Dannenberg
 * April 2023
 */

#include <stdio.h>
#include <o2.h>
#include "audioblock.h"

Audioblock *audioblock_alloc(int chans)
{
    long bytes = sizeof(Audioblock) + 
                 sizeof(int16_t) * chans * AUDIOBLOCK_FRAMES;
    Audioblock *ab = (Audioblock *) O2_MALLOC(bytes);
    printf("audioblock_alloc: %d frames, %ld bytes, %ld%% internal frag\n",
           AUDIOBLOCK_FRAMES, bytes, (long)
           (100 * o2_allocation_size(ab, bytes) + bytes / 2) / bytes - 100);
    ab->frames = 0;
    ab->channels = chans;
    ab->last = false;
    return ab;
}
