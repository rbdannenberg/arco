/* strplay.h -- unit generator to stream from audio file
 *
 * Roger B. Dannenberg
 * Apr 2023
 */

extern const char *Strplay_name;

class Strplay : public Ugen {
public:
    bool started;  // has been started
    bool stopped;  // has been stopped (or finished)
    Audioblock *blocks[2];  // pointers to buffers
    int block_on_deck;  // the block to play from
    int frame_in_block; // current offset in block_on_deck
    int next_block;  // where to put next block from reader
    bool mix;
    bool expand;

   Strplay(int id, const char *filename, int nchans, float start, float end,
           bool cycle, bool mix_, bool expand_) : Ugen(id, 'a', nchans) {
        started = false;
        stopped = false;
        mix = mix_;
        expand = expand_;
        started = false;
        block_on_deck = 0; 
        next_block = 0;
        blocks[0] = NULL;
        blocks[1] = NULL;
        frame_in_block = 0;
        o2sm_send_cmd("/fileio/strplay/new", 0, "isffB", id, filename,
                      start, end, cycle);
    };

    ~Strplay();

    const char *classname() { return Strplay_name; }

    void print_sources(int indent, bool print) { return; }

    void play(bool play) {
        // You can only play once, no play after stopped:
        if (play && !stopped) {
            started = true;
        } else if (!play && !stopped) { // you can stop before play starts!
            stopped = true;
        } else {
            return;
        }
        o2sm_send_cmd("/fileio/strplay/play", 0, "iB", id, play);
    }


    void samps(Audioblock *block) {
        assert(blocks[next_block] == NULL);
        blocks[next_block] = block;
        printf("Strplay::samps: blocks[%d] gets %p\n", next_block, block);
        next_block ^= 1;  // swap 0 <-> 1
    }


    void advance_to_next_block() {
        if (!blocks[block_on_deck]) {
            printf("Warning: playstr underflow\n");
            return;
        }
        if (blocks[block_on_deck]->last) {  // end of file
            stopped = true;
            o2sm_send_cmd("/fileio/strplay/stop", 0, "i", id);
            printf("Strplay::advance_to_next_block: last block is %p\n",
                   blocks[block_on_deck]);
        } else {
            blocks[block_on_deck] = NULL;
            block_on_deck ^= 1;  // swap 0 <-> 1
            o2sm_send_cmd("/fileio/strplay/read", 0, "i", id);
            printf("Strplay::advance_to_next_block: blocks[%d] is %p\n",
                   block_on_deck, blocks[block_on_deck]);
        }
    }

    void real_run() {
        Audioblock *block = blocks[block_on_deck];
        if (!started || stopped || !block) {
            memset(out_samps, 0, chans * BL * sizeof(float));
            return;
        }
        int i = 0;  // how many frames we have computed so far
        // how many channels to copy from input to output:
        int nchans = imin(chans, block->channels);
        
        while (i < BL) {  // may execute 2x to read from next block
            if (!block) {  // zero the remaining output frames
                for (int ch = 0; ch < nchans; ch++) {
                    float *out = out_samps + ch * BL + i;
                    for (int j = i; j < BL; j++) {
                        *out++ = 0;
                    }
                }
                break;
            }
            int nframes = imin(BL - i, block->frames - frame_in_block);
            int16_t *in_base_ptr = &(block->dat[
                    (frame_in_block + i ) * block->channels]);
            for (int ch = 0; ch < nchans; ch++) {
                float *out = out_samps + ch * BL + i; // output not interleaved
                int16_t *inptr = in_base_ptr + ch;
                for (int f = 0; f < nframes; f++) {
                    *out++ = *inptr * INT16_TO_FLOAT_SAMPLE;
                    inptr += block->channels;
                }
            }
            // if there are more input channels and mix is set, add extra chans:
            if (mix) {
                for (int ch = chans; ch < block->channels; ch++) {
                    int outch = ch % chans;
                    float *out = out_samps + outch * BL + i;
                    int16_t *inptr = in_base_ptr + ch;
                    for (int f = 0; f < nframes; f++) {
                        *out++ += *inptr * INT16_TO_FLOAT_SAMPLE;
                        inptr += block->channels;
                    }
                }
            }
            frame_in_block += nframes;
            i += nframes;
            if (frame_in_block == block->frames) {
                printf("strplay block %lld ", current_block);
                advance_to_next_block();
                block = &block[block_on_deck];
                frame_in_block = 0;
            }
        }
        // if we have not written all the channels yet...
        if (nchans < chans) {
            if (expand) {  // copy from earlier channels because
                           // block->chans < chans
                for (int ch = nchans; ch < chans; ch++) {
                    float *out = out_samps + ch * BL;
                    float *src = out_samps + (ch % nchans) * BL;
                    memcpy(out, src, BL * sizeof(float));
                }
            } else {  // don't "expand" input; instead, fill with zeros
                for (int ch = nchans; ch < chans; ch++) {
                    float *out = out_samps + ch * BL;
                    memset(out, 0, BL * sizeof(float));
                }
            }
        }
    }
};

