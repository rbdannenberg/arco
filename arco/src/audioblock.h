#ifndef AUDIOBLOCK_H
#define AUDIOBLOCK_H

// what's a good size? 8000 was experiencing file read underflows.
// 16000 is 1/3 second at least, and for mono, it's 32KB.
#define AUDIOBLOCK_FRAMES 16000

typedef struct  {
    int32_t frames;    // <= blocksize/channels, measured in frames, not samples
    int16_t channels;  // how many channels, 0 means error opening source
    int16_t last;      // is this the last block?
    int16_t dat[0];    // really dat[frames * channels]
} Audioblock;


Audioblock *audioblock_alloc(int chans);


#endif // AUDIOBLOCK_H
