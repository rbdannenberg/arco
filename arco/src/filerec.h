/* fileplay.h -- unit generator to stream from audio file
 *
 * Roger B. Dannenberg
 * Apr 2023
 */

/* References and reference counting: see fileplay.h for overview. */

extern const char *Filerec_name;
extern O2sm_info *fileio_bridge;

void send_filerec_write(int64_t addr, Audioblock *block_ptr);

class Filerec : public Ugen {
public:
    bool isready;  // file is open for writing
    bool recording;  // we are recording incoming samples
    bool stopped;  // we have stopped recording
    bool full;   // recording buffers are both full or busy
    bool finished;  // the fileio object is finished and deleted
    Audioblock *blocks[2];  // pointers to buffers
    int block_on_deck;  // the block to write to
    int frame_in_block; // current offset in block_on_deck
    int next_block;  // the next block the writer will finish writing
    int action_id;  // send this when file is open for writing
    int num_ready_to_send;    // how many blocks are waiting to be sent?
    int num_free;  // how many blocks are available to be written?
    int block_to_send;  // the block to send next
    
    
    Ugen_ptr inp;
    int inp_stride;
    Sample_ptr inp_samps;


    Filerec(int id, int chans, const char *filename, Ugen_ptr inp) :
            Ugen(id, 0, chans) {
        isready = false;
        recording = false;
        stopped = false;
        full = false;
        blocks[0] = audioblock_alloc(chans);
        blocks[1] = audioblock_alloc(chans);
        block_on_deck = 0; 
        frame_in_block = 0;
        next_block = 0;
        action_id = 0;
        num_ready_to_send = 0;
        num_free = 1;
        block_to_send = 0;

        init_inp(inp);

        o2_send_start();
        o2_add_int64((int64_t) this);
        o2_add_int32(chans);
        o2_add_string(filename);
        O2message_ptr msg = o2_message_finish(0.0, "/fileio/filerec/new",
                                              true);
        fileio_bridge->outgoing.push((O2list_elem *) msg);
    };

    ~Filerec();

    void unref() {
        refcount--;
        if (refcount == 0) {
            if (!finished) {
                record(false);
            } else {
                printf("Filerec::unref deleting %p\n", this);
                delete this;
            }
        }
    }

    const char *classname() { return Filerec_name; }

    void print_sources(int indent, bool print) { return; }

    void init_inp(Ugen_ptr ugen) { init_param(ugen, inp, inp_stride); }


    void repl_inp(Ugen_ptr inp_) {
        inp->unref();
        init_inp(inp_);
    }


    void record(bool record) {
        // You can only record once, no recording after stopped:
        if (record && !stopped) {
            recording = true;
        } else if (!record && !stopped) { // you can stop before play starts!
            stopped = true;
            if (!full) {  // file is open, write the last block
                advance_to_next_block(true);
            }                
        } else {
            return;
        }
    }


    // this is a notice from Fileio that it is ready to start.
    void ready(bool ready_flag) {
        if (isready) {
            arco_warn("Filerec - unexpected ready message (ignored)");
            return;
        }
        if (ready_flag) {
            isready = true;
            if (action_id) {
                send_action_id(action_id, 0);  // successful open
            }
            if (recording) {  // do we need to send block(s)?
                while (num_ready_to_send > 0) {
                    send_a_block();
                }
            }
        } else {
            arco_warn("Filerec - failure to open file");
            if (action_id) {
                send_action_id(action_id, -1);
            }
            finished = true;
        }
    }


    void send_a_block() {
        o2_send_start();
        o2_add_int64((int64_t) this);
        o2_add_int64((int64_t) blocks[block_to_send]);
        printf("filerec send_a_block: block %p block dat %p\n",
               blocks[block_to_send], blocks[block_to_send]->dat);
        O2message_ptr msg =
                o2_message_finish(0.0, "/fileio/filerec/write", true);
        fileio_bridge->outgoing.push((O2list_elem *) msg);
        num_ready_to_send--;
        block_to_send ^= 1;
    }


    void samps() {
        num_free++;
        if (blocks[next_block]->last) {
            printf("filerec samps got last block %d\n", next_block);
            finished = true;
            if (refcount == 0) {
                delete this;
            }
        }
        next_block ^= 1;  // swap 0 <-> 1
        if (full) {  // we can now start filling a new buffer
            assert(next_block == block_on_deck);
            num_free--;
            block_on_deck ^= 1;
            frame_in_block = 0;
            full = false;
            printf("filerec full is false\n");
        }
    }


    void advance_to_next_block(bool last) {
        num_ready_to_send++;
        blocks[block_on_deck]->frames = frame_in_block;
        blocks[block_on_deck]->last = last;
        if (isready) {
            send_a_block();
        }
        if (num_free == 0 && !stopped) {
            full = true;
            printf("Warning: filerec overflow\n");
            return;
        }
        num_free--;
        block_on_deck ^= 1;  // swap 0 <-> 1
        frame_in_block = 0;
    }


    void real_run() {
        Audioblock *block = blocks[block_on_deck];
        inp_samps = inp->run(current_block);  // update input
        if (stopped || !recording || full) {  // nothing we can do with input
            return;
        }
        // read one frame at a time and write to block
        int16_t *out = &(block->dat[frame_in_block * block->channels]);
        if (frame_in_block == 0) {
            printf("real_run, frame_in_block == 0, write to %p (out) dat %p\n",
                   out, &(block->dat[0]));
        }
        for (int i = 0; i < BL; i++) {
            Sample_ptr in = inp_samps + i;
            for (int ch = 0; ch < chans; ch++) {
                *out++ = FLOAT_TO_INT16(*in);
                in += inp_stride;
            }
        }
        frame_in_block += BL;
        assert(frame_in_block <= AUDIOBLOCK_FRAMES);
        // see if there's room for another BL frames:
        if (frame_in_block + BL * block->channels > AUDIOBLOCK_FRAMES) {
            advance_to_next_block(false);
        }
    }
};

