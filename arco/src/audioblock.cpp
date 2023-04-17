/* audioblock.cpp - audio blocks are shared memory use for audio file IO
 *
 * Roger B. Dannenberg
 * April 2023
 */

#include "audioblock.h"

Audioblock *asynchio_alloc(int chans)
{
    Audioblock *ab = O2_MALLOC(sizeof(Audioblock) + 
                               sizeof(int16) * chans * 8000);
    ab->channels = chans;
    ab->last = false;
    return ab;
}
