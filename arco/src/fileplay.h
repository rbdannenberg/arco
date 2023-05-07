/* fileplay.h -- unit generator to stream from audio file
 *
 * Roger B. Dannenberg
 * Apr 2023
 */

/* References and reference counting:

A Fileplay unit generator uses a Fileio_obj in another thread to read
the file. There can be multiple Fileplay and Fileio_obj objects, so we need a way for Fileplay and Fileio_obj objects to reference each other.

Imagine that we use ordinary Arco reference counting. Fileplay sends
its id to Fileio, which uses a mapping from id to Fileio_obj to locate
the Filio_obj. Reply messages carry the id to locate the Fileplay
object. Since there is an outstanding id, we need to increment the
reference count (otherwise, the Fileplay object could be deleted and
replaced by something else, and an incoming message from Fileio_obj
could be directed to the "something else" object. This is essentially
a cycle in the reference counting scheme. The only way to break the
cycle is to explicitly remove the Fileio_obj and its reference, but
the whole point of reference counting is to clean up automatically.

An alternative is keep a separate id for Fileio_obj so that reference
counting can be used to detect when the Fileplay is no longer
needed. Then, the problem is to make sure Fileio_obj is deleted and
any in-flight messages are received before actually deleting Fileplay.

The id is the address of the Fileplay object. On the Fileio side, we
simply do a linear search to map id to object (we assume the search
time is negligible compared to file IO operations). The address is
simply coerced from 64-bit integer to a Fileplay * on the Arco side.

We override Fileplay::unref() so that when the refcount goes to zero,
we delete the Fileio_obj and do nothing more until receiving a
confirmation, when we can delete the Fileplay object. The Fileio_obj
is deleted with an /fileio/fileplay/play message (play = false). The
confirmation message is /arco/fileplay/ready (ready = false).

*/

extern const char *Fileplay_name;
extern O2sm_info *fileio_bridge;

void send_fileplay_play(int64_t addr, bool play_flag);

class Fileplay : public Ugen {
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

   Fileplay(int id, const char *filename, int nchans, float start, float end,
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

        // o2sm_send_cmd("/fileio/fileplay/new", 0, "hsffB", addr, filename,
        //               start, end, cycle);
        o2_send_start();
        o2_add_int64((int64_t) this);
        o2_add_string(filename);
        o2_add_float(start);
        o2_add_float(end);
        o2_add_bool(cycle);
        O2message_ptr msg = o2_message_finish(0.0, "/fileio/fileplay/new",
                                              true);
        fileio_bridge->outgoing.push((O2list_elem *) msg);
    };

    ~Fileplay();

    void unref() {
        refcount--;
        if (refcount == 0) {
            if (!stopped) {
                play(false);
            } else {
                printf("Fileplay::unref deleting %p\n", this);
                delete this;
            }
        }
    }

    const char *classname() { return Fileplay_name; }

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

        // o2sm_send_cmd("/fileio/fileplay/play", 0, "hB", addr, play);
        send_fileplay_play((int64_t) this, play);
    }


    // this is a notice from Fileio that it is ready to start.
    void ready(bool is_ready) {
        if (!is_ready && !started) {
            arco_warn("Fileplay - failure to start reading from file");
            if (action_id) {
                send_action_id(action_id, -1);  // send error code
            }
        }
        if (!is_ready) {  // there is nothing more to read
            stopped = true;
            if (refcount == 0) {
                printf("Fileplay::ready deleting %p\n", this);
                delete this;
            }
        }
    }

    
    void set_action_id(int id) {
        action_id = id;
        if (stopped) {
            send_action_id(action_id, started ? 0 : -1);
        }
    }

    void samps(Audioblock *block) {
        assert(blocks[next_block] == NULL);
        blocks[next_block] = block;
        next_block ^= 1;  // swap 0 <-> 1
    }


    Audioblock *advance_to_next_block() {
        if (blocks[block_on_deck]->last) {  // not end of file
            play(false);
        } else if (!stopped) {
            // o2sm_send_cmd("/fileio/fileplay/read", 0, "h", addr);
            o2_send_start();
            o2_add_int64((int64_t) this);
            O2message_ptr msg = 
                    o2_message_finish(0.0, "/fileio/fileplay/read", true);
            fileio_bridge->outgoing.push((O2list_elem *) msg);
        }
        blocks[block_on_deck] = NULL;
        block_on_deck ^= 1;  // swap 0 <-> 1
        Audioblock *block = blocks[block_on_deck];
        if (!block && !stopped) {
            printf("Warning: fileplay underflow\n");
        }
        frame_in_block = 0;
        return block;
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
                    *out++ = INT16_TO_FLOAT(*inptr);
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
                        *out++ += INT16_TO_FLOAT(*inptr);
                        inptr += block->channels;
                    }
                }
            }
            frame_in_block += nframes;
            i += nframes;
            if (frame_in_block == block->frames) {
                block = advance_to_next_block();
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

