// probe.h -- probe for software oscilloscope
//
// Roger B. Dannenberg
// May 2023

/* probe is an audio unit generator that sends samples to 
   another object using a set_vector message. You can capture 
   every Nth sample using stride. 

   Usage: Initialize with /arco/probe/new id chans input_id reply_to, where
       reply_to is a full O2 address to receive messages consisting
       of probe id and zero to 64 floats.

   To get samples: send /arco/probe/probe with
       id = probe id number
       period = time between messages. Send -1 for a one-shot probe.
           For any period >= 0, periods are measured relative to the
           *first* frame of a message, but actual period is at least
           frames * stride (no overlapping frames) rounded up to the
           nearest block, and if thresholding
           is enabled, the search for a threshold *begins* after the
           period has elapsed.
       frames = number of frames to send each period. The maximum
           length of a message is 64, so there will be a sequence
           of ceiling(frames * chans / 64) messages sent each period.
       chan = first channel to probe
       nchans = how many channels to probe. Channels are interleaved
           in the message.
       stride = capture every Nth frame
       max_wait = time in seconds to look at for a threshold
           crossing in the first channel. If zero, wait forever.

   To wait for a threshold crossing (e.g. for an oscilloscope):
       Send /arco/probe/thresh with
           float threshold = the threshold value
           int32 direction = 1 for positive and -1 for negative, 0 for no
               threshold detection, run immediately
       Then send /arco/probe/probe as above. Use max_wait to control how
           long to wait for a zero crossing:

           DESIRED BEHAVIOR        MAX_WAIT     DIRECTION
           wait until threshold    zero         non-zero
           wait up to max_wait     non-zero     non-zero
           do not wait             *            zero

   To stop waiting for probe messages, send /arco/probe/stop
       When an empty (no floats) message arrives, the probe has been reset.

   To get periodic snapshots, set period to seconds; period
       will be quantized to blocks, and period may not be uniform if
       threshold detection is enabled
*/

#define PROBE_IDLE 0
#define PROBE_WAITING 1
#define PROBE_COLLECTING 2
#define PROBE_DELAYING 3

extern const char *Probe_name;

class Probe : public Ugen {
  public:
    Ugen_ptr input;
    int input_stride;
    Sample_ptr input_samps;

    // need 24 bytes overhead + up to 68 bytes for type string with
    // 64 floats; allow 64 bytes for address -> 156, add 4 extra:
    char msg_header[160];  // we prebuild an outgoing message here
    char *typestring_ptr; // where to put typestring in msg_header
    int msg_header_len;
    O2message_ptr msg;     // message data is here to accumulate samples
    int msg_len;
    Sample *sample_ptr;    // samples are accumulated here
    Sample *sample_fence;  // address beyond last sample in message

    float period;     // how often to send samples
    int frames;       // requested number of samples per period - this
        // message lengths are all the same in between calls to probe,
        // and zero padding is used if message lengths (64 / channels)
        // do not evenly divide frames.
    int frames_sent;  // how many frames have been sent this period?
    int channel_offset; // what channel we start reading from
    int channels;     // how many channels to read
    int stride;       // how many inputs samples to skip per output sample
    int repeats;      // how many messages to send
    Sample threshold; // start collecting on theshold crossing
    int direction;    // crossing direction, +1, -1, or 0 for don't wait
    float max_wait;   // how long to wait for threshold crossing
    int next;  // index of next sample for buffer relative to 
               // beginning of the current block, e.g. if next = 34,
               // wait for one 32-sample block and take the 2nd sample
    int frames_per_msg;  // how many frames go into each message
    int wait_count;  // counts how many blocks before auto sweep
    int wait_in_blocks; // saves wait_count for subsequent vectors
    int delay_in_blocks; // saves wait_count for subsequent vectors
    int64_t prev_start_block; // when did last period start?
    int state;      // state: idle, waiting, collecting, 
                    //        delaying until next period
    Sample prev_sample; // one-sample history for threshold crossing detection
    

    Probe(int id, Ugen_ptr input, char *reply_addr) : Ugen(id, 0, 1) {
        O2message_ptr m = (O2message_ptr) msg_header;
        m->next = 0;
        m->data.misc = 0;
        m->data.timestamp = 0;
        int len = (int) strlen(reply_addr);
        // msg_header has 96 bytes: next (8), length (4), misc (4),
        //     timestamp (8), address (?), types (4). 96 - 28 = 68
        if (len > 63) {
            len = 63;
        }
        memcpy(m->data.address, reply_addr, len);
        m->data.address[len++] = 0;
        while (len & 3) m->data.address[len++] = 0;  // fill to 32 boundary
        typestring_ptr = m->data.address + len;
        *((int32_t *) typestring_ptr) = 0;  // fill whole word
        *typestring_ptr++ = ',';  // first character of all O2/OSC typestrings
        *typestring_ptr++ = 'i';  // first parameter is our index
        // now, typestring_ptr is where to finish writing type string

        state = PROBE_IDLE;
        frames_sent = 0;
        msg = NULL;
        // default values:
        threshold = 0.0;
        direction = 1;
        max_wait = 0.02;
        prev_sample = 0.0;

        init_input(input);
    }


    ~Probe() {
        printf("~Probe destructor called, id %d; does nothing.\n", id);
    }

    const char *classname() { return Probe_name; }

    void repl_input(Ugen_ptr ugen) {
        input->unref();
        init_input(ugen);
    }

    void print_details(int indent) {
        arco_print("period %g frames %d channels %d stride %d repeats %d\n",
                   period, frames, channels, stride, repeats);
        indent_spaces(indent + 2);
        arco_print("threshold %g direction %d max_wait %g",
                   threshold, direction, max_wait);
    }

    
    void print_sources(int indent, bool print_flag) {
        input->print_tree(indent, print_flag, "input");
    }


    void init_input(Ugen_ptr ugen) {
        init_param(ugen, input, input_stride);
        // TODO: we should fail gracefully in this case:
        assert(input->rate == 'a' || input->rate == 'b');
        stop();  // after input change we need another probe() to start again
    }

    void prepare_msg() {
        msg = (O2message_ptr) O2_MALLOC(msg_len);
        memcpy(msg, msg_header, msg_header_len);
        sample_ptr = (float *) (((char *) msg) + msg_header_len);
        sample_fence = (float *) (((char *) msg) + msg_len);
    }
    

    void probe(float period_, int frames_, int chan, 
               int nchans, int stride_) {
        stop();  // make sure we are stopped
        if (!input) {
            return;
        }

        period = period_;
        delay_in_blocks = (int) (period * BR);
        wait_in_blocks = (int) (max_wait * BR);
        if (wait_in_blocks == 0 && max_wait > 0) {  // unless max_wait is zero,
            wait_in_blocks = 1;                  // wait_in_blocks must be >= 1
        }
        state = PROBE_WAITING;
        wait_count = wait_in_blocks;

        // this is very important: the threshold tests depend upon prev_sample
        // being either LESS THAN or GREATER THAN threshold, so here we make
        // it equal. The first test will fail, forcing a true signal value
        // to be stored in prev_sample. In order to make sure the compare
        // of two floats is EQUAL, threshold should be a float, not a double.
        prev_sample = threshold;

        frames = frames_;
        frames_sent = 0;
        channel_offset = chan;
        if (channel_offset >= input->chans || channel_offset < 0) {
            channel_offset = 0;
        }
        channels = nchans;
        if (channel_offset + channels > input->chans) {
            channels = input->chans - channel_offset;
        }
        if (channels < 1) channels = 1;

        stride = stride_;
        if (stride < 1) stride = 1;
        if (stride * frames > (long) AR * 5) {  // this will take >5 sec
            arco_warn("Probe spans %d samples!", stride * frames);
        }

        next = 0;

        // complete msg_header
        if (frames < 0) {
            frames = 0;
        }
        frames_per_msg = frames;
        if (frames_per_msg * channels > 64) {  // max msg has 64 floats
            frames_per_msg = 64 / channels;
        }
        char *types = typestring_ptr;
        for (int i = 0; i < frames_per_msg * channels; i++) {
            *types++ = 'f';
        }
        *types++ = 0;  // need at least one zero before word boundary
        while ((uintptr_t) types & 3) *types++ = 0;  // zero fill word

        msg_header_len = (int) (types - msg_header);
        assert(msg_header_len <= 160);

        // + 1 because the first parameter is (integer) id
        msg_len = (int) (msg_header_len + 4 * (frames_per_msg * channels + 1));
        *((int32_t *) types) = id; // first msg arg is our id
        msg_header_len += 4;  // because it now contains 1st parameter: id

        if (frames_per_msg == 0) {  // too many channels for even one frame
            frames = 0;
            msg = NULL;
            state = PROBE_WAITING;  // in order to get stop() to send a message
            stop();
        }
    }


    void thresh(float threshold_, int direction_, float max_wait_) {
        printf("Probe::thresh threshold %g, direction %d, max_wait %g\n",
               threshold_, direction_, max_wait_);
        threshold = threshold_;
        prev_sample = threshold;
        direction = direction_;
        max_wait = max_wait_;
    }
    

    void stop() {
        if (state != PROBE_IDLE) {
            o2sm_send_cmd(((O2message_ptr) msg_header)->data.address, 0, "i", id);
        }
        if (msg) {
            O2_FREE(msg);
            msg = NULL;
        }
        state = PROBE_IDLE;
   }

    
    virtual void real_run();
};
