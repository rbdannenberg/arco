/* strplay.h -- unit generator to stream from audio file
 *
 * Roger B. Dannenberg
 * Apr 2023
 */

extern const char *Strplay_name;
extern O2sm_info *fileio_bridge;

void send_strplay_play(int64_t addr, bool play_flag);

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
    int action_id;  // send this when playback is stopped or finished
    int reads_outstanding;  // how many unanswered read requests?

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
        action_id = 0;

        // o2sm_send_cmd("/fileio/strplay/new", 0, "hsffB", addr, filename,
        //               start, end, cycle);
        o2_send_start();
        o2_add_int64((int64_t) this);
        o2_add_string(filename);
        o2_add_float(start);
        o2_add_float(end);
        o2_add_bool(cycle);
        O2message_ptr msg = o2_message_finish(0.0, "/fileio/strplay/new",
                                              true);
        fileio_bridge->outgoing.push((O2list_elem *) msg);

        ref();  // we expect a samps message from Fileio
        ref();  // we expect a ready message from Fileio
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
        ref();  // sending ref to Fileio, expect samps reply

        // o2sm_send_cmd("/fileio/strplay/play", 0, "hB", addr, play);
        send_strplay_play((int64_t) this, play);
    }


    // this is a notice from Fileio that it is ready to start.
    void ready(bool is_ready) {
        unref();
    }


    void samps(Audioblock *block) {
        assert(blocks[next_block] == NULL);
        blocks[next_block] = block;
        next_block ^= 1;  // swap 0 <-> 1
        // when read request completes, the reference sent to Fileio is returned
        unref();
    }


    void advance_to_next_block() {
        if (!blocks[block_on_deck]) {
            printf("Warning: playstr underflow\n");
            return;
        }
        if (blocks[block_on_deck]->last) {  // end of file
            stopped = true;
            send_strplay_play(id, false);
        } else {
            blocks[block_on_deck] = NULL;
            block_on_deck ^= 1;  // swap 0 <-> 1

            // o2sm_send_cmd("/fileio/strplay/read", 0, "h", addr);
            o2_send_start();
            o2_add_int64((int64_t) this);
            O2message_ptr msg = 
                    o2_message_finish(0.0, "/fileio/strplay/read", true);
            fileio_bridge->outgoing.push((O2list_elem *) msg);

            ref();  // sending a reference (our id) to Fileio
        }
    }

    void real_run() {
        Audioblock *block = blocks[block_on_deck];
        if (!started || stopped || !block) {
            memset(out_samps, 0, chans * BL * sizeof(float));
            if (stopped && action_id) {
                send_action_id(action_id);
            }
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

