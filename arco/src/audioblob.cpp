// audioblob.cpp -- support for sending audio in blobs over O2
//
// Roger B. Dannenberg
// Aug 2024

#include "assert.h"
#include <string.h>
#include "o2internal.h"
#include "sharedmemclient.h"
#include "arcotypes.h"
#include "arcoutil.h"
#include <cmath>
#include "ugenid.h"
#include "audioio.h"
#include "ugen.h"
#include "audioblob.h"

void blob_byteswap(char *data, int frames, int chans, bool floattype)
{
#if IS_LITTLE_ENDIAN
    int n = frames * chans;
    if (floattype) {
        for (int i = 0; i < n; i++) {
            int32_t *loc = ((int32_t *)
                            (&data[i * sizeof(float)]));
            int32_t f = *loc;
            *loc = swap32(f);
        }
    } else {
        for (int i = 0; i < n; i++) {
            int16_t *loc = ((int16_t *)
                            (&data[i * sizeof(int16_t)]));
            int16_t f = *loc;
            *loc = swap16(f);
        }
    }
#endif
}
