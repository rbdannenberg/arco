/* recplay -- unit generator for arco
 *
 * based on irecplay.h from AuraRT
 *
 * Roger B. Dannenberg
 * Apr 2023
 */

#ifndef __Recplay_H__
#define __Recplay_H__

// INTERFACE:
// set_in -- the audio input
// set_fade -- the fade-in == fade_out time for playback, must be >= 0;
//             fade-out time is equal to fade-in time even if you change
//             the fade value after play begins
// record -- begins recording from time zero when this value becomes non-zero
// play --  set starting time for playback and begin playing
// stop -- trigger to stop playback
// set_loop -- boolean selects whether to loop or stop at end. Loops do not
//             cross fade. Instead, you fade out, then fade in from the starting
//             point (the original start_from parameter passed to play method)
// borrow -- use samples from another irecplay object to allow multiple
//           simultaneous playback
// record and playback can be simultaneous

// recording is stored in a dynamic array of buffers. Each buffer
// holds 32K samples or about 0.7s of mono. Samples are stored in
// blocks of size BL. This allows us to transfer one block at a time.

#define LOG2_BLOCKS_PER_BUFFER 10
#define LOG2_SAMPLES_PER_BUFFER (LOG2_BLOCKS_PER_BUFFER + LOG2_BL)
#define SAMPLES_PER_BUFFER (1 << LOG2_SAMPLES_PER_BUFFER)


// INTERNAL STATE:
//     rec_index -- integer offset
//     sample_count -- integer, how many samples can we play?
//                  (determined by previous recording length)
//     play_index -- integer offset
//     recording -- true when recording
//     playing -- true when playing
//     fading -- true when fading in or out
//     stopping -- true when fading out
//     lender_ptr -- where (and if) to find borrowed samples

#include <climits>



extern const char *Recplay_name;

class Recplay : public Ugen {
public:
    struct Recplay_state {
        Vec<Sample_ptr> my_buffers;  // every Recplay has a place to put buffers

        // but actual used buffers can be "borrowed" from another
        // Recplay allowing multiple players for a single
        // recording. To support this, we play from buffers, which can
        // point to my_buffers or lender's my_buffers. If lender is mono
        // and we are multi-channel, the lender's buffer can be shared
        // by all of our channels:
        Vec<Sample_ptr> *buffers;
        Sample prev_sample; // for linear interpolation
    };
    Vec<Recplay_state> states;
    
    // all processing across channels is synchronous, so most of the state
    // is common to all channels:

    double start_time;
    int action_id;  // id to send when playback completes
    bool loop;
    long rec_index;     // total number of *samples* recorded
    long play_index;    // next *sample* index to play
    double play_phase;  // like play_index, but a double
    float speed;        // speed up or slow down factor for playback
    long sample_count;  // count of recorded samples
    long loan_count;    // how many are borrowing our buffers
    bool borrowing;     // true iff we are borrowing someone's buffers
    Recplay *lender_ptr;  // buffer lender
    bool recording;
    bool playing;
    bool fading;
    bool stopping;
    float fade;         // fade in/out time in seconds
    float fade_incr;    // increment for fade-in, fade-out
    float fade_phase;   // index into raised cosine
    long fade_len;      // in samples before adjusting by speed, so this is
                        //    basically fade time in output samples
    long fade_count;    // how many blocks remain in fade-in or fade-out

    // to minimize internal fragmentation, we try to use everything allocated
    // (but do not split blocks of samples). We need to know the actual
    // allocation size, which will be consistent. Note that SAMPLES_PER_BUFFER
    // is the *requested* size, and samples_per_buffer is the *actual* number
    // of samples we will use -- it is greater or equal to SAMPLES_PER_BUFFER
    // and also a multiple of BLOCK_BYTES so we can transfer a block at at time:
    int samples_per_buffer;
        
    Ugen_ptr input;
    int input_stride;
    Sample_ptr input_samps;

    Ugen_ptr gain;
    int gain_stride;
    Sample_ptr gain_samps;


    Recplay(int id, int nchans, Ugen_ptr input, Ugen_ptr gain,
            float fade_, bool loop_) : Ugen(id, 'a', nchans) {
        action_id = 0;
        tail_blocks = 1;  // (I think) we terminate when last samples are
                          // output, so on the *next* block, output will be zero
        fade = fade_;
        loop = loop_;
        sample_count = 0;
        loan_count = 0;
        lender_ptr = NULL;
        speed = 1.0;
        borrowing = false;
        recording = false;
        playing = false;
        fading = false;
        stopping = false;
        states.set_size(chans);

        for (int i = 0; i < chans; i++) {
            states[i].buffers = NULL;
            // this will be reinitialized if we do not borrow buffers from
            // another Recplay.  Reinitialization is OK if the initial size
            // is zero because nothing is allocated. (see o2/src/vec.h):
            states[i].my_buffers.init(0);
            states[i].prev_sample = 0;
        }
        init_input(input);
        init_gain(gain);

        // allocate a block and capture its actual size
        Sample *buffer = O2_MALLOCNT(SAMPLES_PER_BUFFER, Sample);
        samples_per_buffer = ((int) (o2_allocation_size(buffer,
                              SAMPLES_PER_BUFFER / BL) / BLOCK_BYTES)) * BL;
        O2_FREE(buffer);  // we just needed it so get the true allocation size
    }

    ~Recplay() {
        int i;
        input->unref();
        gain->unref();
        if (loan_count) {
            arco_error("Recplay::~Recplay -- loan_count non-zero!");
            return;
        } else if (lender_ptr) {
            lender_ptr->unref();
            return;  // don't free buffers, they belong to lender
        }
        for (int chan = 0; chan < chans; chan++) {
            Vec<Sample_ptr> &buffers = states[chan].my_buffers;
            for (int i = 0; i < buffers.size(); i++) {
                O2_FREE(buffers[i]);
            }
            buffers.finish();
        }
        states.finish();
    }

    const char *classname() { return Recplay_name; }

    void print_details(int indent) {
        arco_print("rec %s, play %s speed %g",
                   recording ? "true" : "false", playing ? "true" : "false",
                   speed);
    }


    void print_sources(int indent, bool print_flag) {
        input->print_tree(indent, print_flag, "input");
        gain->print_tree(indent, print_flag, "gain");
    }

    void repl_input(Ugen_ptr ugen) {
        input->unref();
        init_input(ugen);
    }

    void repl_gain(Ugen_ptr ugen) {
        gain->unref();
        init_gain(ugen);
    }

    void set_gain(int chan, float g) {
        gain->const_set(chan, g, "Recplay::set_gain");  }
    
    void set_speed(float speed_) { speed = speed_;  }

    void init_input(Ugen_ptr ugen) {
        assert(ugen->rate == 'a');
        init_param(ugen, input, input_stride);  }

    void init_gain(Ugen_ptr ugen) {
        assert(ugen->rate != 'a');  // allow 'c' and (non-interpolated) 'b'
        init_param(ugen, gain, gain_stride);  }

    void record(bool record);

    void start(double start_time);

    void stop();

    void borrow(int lender_id);

    void play_fade_1(int chan, float fade_phase, int buffer, int offset);

    void play_fade_x(int chan, float fade_phase, int buffer, int offset,
                     double phase);

    void play_1(int i, int buffer, int offset);

    void play_x(int i, int buffer, int offset, double phase);

    void real_run();
};

#endif
