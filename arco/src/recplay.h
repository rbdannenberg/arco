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

#define COS_TABLE_SIZE 100
extern float raised_cosine[COS_TABLE_SIZE + 5];


extern const char *Recplay_name;

class Recplay : public Ugen {
public:
    double start_time;
    int action_id;  // id to send when playback completes
    bool loop;
    long rec_index;
    long play_index;
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
    Sample prev_sample; // for linear interpolation
    float fade_incr;    // increment for fade-in, fade-out
    float fade_phase;   // index into raised cosine
    long fade_len;      // in samples before adjusting by speed, so this is
                        //    basically fade time in output samples
    long fade_count;    // how many blocks remain in fade-in or fade-out
    Vec<Sample *> my_buffers;  // every Recplay has a place to put buffers

    // but actual used buffers can be "borrowed" from another Recplay
    // allowing multiple players for a single recording. To support this, we
    // play from buffers, which can point to my_buffers or lender->my_buffers:
    Vec<Sample *> *buffers; 
        
    Ugen_ptr inp;
    int inp_stride;
    Sample_ptr inp_samps;

    Ugen_ptr gain;
    int gain_stride;
    Sample_ptr gain_samps;

    void return_buffers() { loan_count--; }



    Recplay(int id, int nchans, Ugen_ptr inp, Ugen_ptr gain, 
            float fade_, bool loop_) : Ugen(id, 'a', nchans) {
        action_id = 0;
        buffers = NULL;
        fade = fade_;
        loop = loop_;
        sample_count = 0;
        loan_count = 0;
        lender_ptr = NULL;
        speed = 1.0;
        recording = false;
        playing = false;

        init_inp(inp);
        init_gain(gain);
    }

    ~Recplay() {
        int i;
        inp->unref();
        gain->unref();
        if (loan_count) {
            arco_error("Recplay::~Recplay -- loan_count non-zero!");
            return;
        } else if (lender_ptr) {
            lender_ptr->return_buffers();
            lender_ptr->unref();
            return;  // don't free buffers, they belong to lender
        }
        for (i = 0; i < buffers->size(); i++) {
            O2_FREE((*buffers)[i]);
        }
    }

    const char *classname() { return Recplay_name; }

    void print_sources(int indent, bool print) {
        inp->print_tree(indent, print, "inp");
        gain->print_tree(indent, print, "gain");
    }

    void repl_inp(Ugen_ptr ugen) {
        inp->unref();
        init_inp(ugen);
    }

    void repl_gain(Ugen_ptr ugen) {
        gain->unref();
        init_gain(ugen);
    }

    void set_gain(int chan, float g) {
        assert(gain->rate == 'c');
        gain->output[chan] = g;  }
    
    void set_speed(float speed_) { speed = speed_;  }

    void init_inp(Ugen_ptr ugen) { 
        assert(ugen->rate == 'a');
        init_param(ugen, inp, inp_stride);  }

    void init_gain(Ugen_ptr ugen) {
        assert(ugen->rate != 'a');  // allow 'c' and (non-interpolated) 'b'
        init_param(ugen, gain, gain_stride);  }

    void record(bool record);

    void start(double start_time);

    void stop();

    void borrow(int lender_id);

    void run_channel(int chan);

    void real_run();
};

#endif