#ifndef AUDIOBLOCK_H
#define AUDIOBLOCK_H

#define AUDIOBLOCK_FRAMES 8000

typedef struct  {
    int32_t frames;    // <= blocksize/channels, measured in frames, not samples
    int16_t channels;  // how many channels, 0 means error opening source
    int16_t last;      // is this the last block?
    int16_t dat[0];    // really dat[frames * channels]
} Audioblock;


Audioblock *audioblock_alloc(int chans);


#endif // AUDIOBLOCK_H
