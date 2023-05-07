/* audioblock.cpp - audio blocks are shared memory use for audio file IO
 *
 * Roger B. Dannenberg
 * April 2023
 */

#include <o2.h>
#include "audioblock.h"

Audioblock *audioblock_alloc(int chans)
{
    Audioblock *ab = (Audioblock *) O2_MALLOC(sizeof(Audioblock) + 
                        sizeof(int16_t) * chans * AUDIOBLOCK_FRAMES);
    ab->frames = 0;
    ab->channels = chans;
    ab->last = false;
    return ab;
}
