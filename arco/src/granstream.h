/* granstream.h -- Granular Synthesis instrument, streaming input */

/* granstream multiplies an envelope by a sample player.
 * Envelopes and sample position, pitch, etc. are
 * controlled by random numbers chosen within a given
 * range.
 * A gate turns it on and off and controls the amplitude.
 * 
 * The algorithm plays a grain every period, where period
 * is selected to achieve the given density. Density may
 * be limited by polyphony.
 * 
 * The algorithm is: compute a grain, delay for a random
 * time, play another grain, etc.
 *
 * Grains are on block boundaries, and on/off ramps are also
 * on block boundaries.
 *
 * There is optional feedback from sum of all output channels
 * to the first channel's input buffer. The feedback is limited
 * using the following sample-by-sample algorithm:
 *    A peak_envelope is instantly set to the peak, and otherwise
 *        decays to 1 with time constant 1 sec.
 *    The peak_smoothed converges to the peak_envelope with time
 *        constant 2 msec.
 *    Time constants are time to converge to 1/e (not 1/2)
 *    Feedback samples are multiplied by 1/peak_smoothed.
 *    Decay is computed by out = in + g * (prev – in), where g is
 *        exp(-1/time_const_in_samples). Note that this produces
 *        the weighted sum g * prev + (1 - g) * in. g is almost 1,
 *        so out is almost prev, but moves toward in.
 * In addition, the feedback inserted into the delay and applied
 * after the delay ramps up and down slowly when feedback is changed
 * to prevent glitches.
 *
 * Feedback is initially zero, and there is no feedback. Delay is
 * also initially zero. When either feedback or delay is set to
 * non-zero, feedback delay is started, and it continues even when
 * feedback becomes zero (although there is a small optimization where
 * output channels are not summed into the feedback if the feedback
 * amount is zero; thus, when feedback becomes non-zero, there will
 * be some delay before feedback is heard. You can avoid this delay
 * by setting feedback to -90dB or at least 0.00003 instead of zero.)
 */

// Normally, I don't like to have "hidden" includes, putting all includes
// to top-level .cpp files so you can see what they depend on, and all
// include files are just included once, but I'm pretty sure no other
// file needs this:
#include <new>

extern const char *Granstream_name;

class Granstream;
class Granstream_state;

enum Gran_state {GS_PREDELAY, GS_RISE, GS_HOLD, GS_FALL};

// a Gran_gen manages a single grain; there are polyphony of these per
// channel, stored in the per-channel state called Granstream_state;
class Gran_gen {
public:
    Gran_state state;     // state machine state (not the per-channel state)
    int delay;           // blocks until next state change
    int attack_blocks;   // grain attack time in blocks
    int release_blocks;  // how many blocks in envelope release
    
    int dur_blocks; // how long is the grain in blocks?
    // phase and ratio (phase increment) are double; otherwise, the cummulative
    // rounding error over a long grain could amount to an error of multiple
    // samples over the course of the grain, causing the phase to drift out of
    // the range of samples stored in the input ring buffer:
    double ratio;   // sample rate conversion: higher ratio -> higher pitch
                    // ratio is uniform random between high and low
    double phase;   // buffer offset relative to now (negative)
    float env_val;  // current envelope value
    float env_inc;  // increment to next envelope value
    float gain;     // gain applied to the current grain

    Gran_gen() {
        state = GS_FALL;
        delay = 1;  // so next time we run, we'll reinitialize
    }

    ~Gran_gen() { ; }

    // returns true if grain is active
    bool run(Granstream *gs, Granstream_state *perchannel,
             Sample_ptr out_samps, int chan, int index);
};


class Granstream_state {  // information for a single channel
public:
    Ringbuf input_buf;   // input buffer (source for grains)
    Vec<Gran_gen> gens;  // individual grains
    int actual_polyphony;  // how many grains to we have to run/are running?

    void init(float dur, int polyphony) {
        int len = ((int) (dur * AR) + BL) & ~(BL - 1);  // round up to BL
        input_buf.init(len, true);
        gens.init(polyphony);  // size is now 0
        gens.set_size(polyphony, false);  // no need to zero fill
        reset_gens(0, polyphony);
        actual_polyphony = polyphony;
    }
    
    void finish() {
        input_buf.finish();
        gens.finish();
    }
    
    // returns true if any grain is active
    bool chan_a(Granstream *gs, Sample *out_samps, int chan);

    void reset_gens(int oldp, int newp) {
        for (int i = oldp; i < newp; i++) {
            // the so-called "placement new" runs constructor given an address:
            new(&gens[i]) Gran_gen;
        }
    }

    void set_polyphony(int oldp, int newp) {
        if (newp > oldp) {
            gens.set_size(newp);
            reset_gens(oldp, newp);
        }
    }

    void set_dur(int len) {  // called by Granstream::set_dur(float dur)
        // note that buffer duration never shrinks -- because we could be
        // reading from a long delay that would become inaccessible if the
        // buffer size is reduced. dur, however, can be reduced so that future
        // grains come from more recent history (lower delay).
        if (len > input_buf.get_fifo_len()) {
            input_buf.set_fifo_len(len, true);
            assert(input_buf.get_fifo_len() == len);
        }
    }
};



class Granstream : public Ugen {
    friend Gran_gen;
public:
    int polyphony;  // how many grains can we have at once per channel
    float dur;      // how far we can reach back in time to find a grain
    bool enable;    // start/stop making grains

    float feedback_delay;    // in seconds
    Ringbuf feedback_buf;  // delay line
    float feedback;
    float peak_envelope;  // used for feedback limiter
    float peak_smoothed;  // used for feedback limiter
    float risefactor;
    float fallfactor;
    float feedbackfactor;
    float feedback_in_smoothed;  // used to smoothly adjust feedback
    float feedback_out_smoothed;  // used to smoothly adjust feedback
    Vec<Granstream_state> states;
    Dcblock dcblock;  // used to block DC in feedback


    Ugen_ptr input;
    int input_stride;
    Sample_ptr input_samps;
    
    float high;     // the upper limit on ratio
    float low;      // the lower limit on ratio
    float highdur;  // the upper limit on grain duration
    float lowdur;   // the lower limit on grain duration
    float density;  // average number of active grains/input channel
    float attack;   // the grain attack time in seconds
    float release;  // the grain release time in seconds
    float gain;     // gain applied to the next grain
    int bufferlen;  // buffer length in samples (based on dur)
    int warning_block;  // used to limit rate of warning messages
    bool stop_request;  // waiting for last grain to finish before setting
                        // enable to false.
    
    Granstream(int id, int nchans, Ugen_ptr input, int polyphony_,
                    float dur_, bool enable_) : Ugen(id, 'a', nchans) {
    /* polyphony - number of grains per channel
     * dur - the duration (s) of the sample buffer per channel
     */
        init_input(input);
        polyphony = polyphony_;
        dur = dur_;
        enable = enable_;
        feedback_delay = 0;
        feedback = 0.0;  // no feedback is default
        feedback_in_smoothed = 0.0;
        feedback_out_smoothed = 0.0;
        peak_envelope = 1.0;
        peak_smoothed = 1.0;
        risefactor = exp(-1.0 / (0.002 * AR));  // 2 msec rise time
        fallfactor = exp(-1.0 / AR);            // 1 sec fall time
        feedbackfactor = exp(-1 / (0.1 * AR));  // 100 msec feedback smoothing
        attack = 0.02;
        release = 0.02;
        high = 1.0;
        low = 1.0;
        highdur = 0.1;
        lowdur = 0.1;
        density = polyphony * 0.5;
        gain = 1.0;
        warning_block = 0;
        stop_request = false;

        states.set_size(chans, false);  // no zero because of following loop:
        for (int i = 0; i < chans; i++) {
            states[i].init(dur, polyphony);
        }
    }

    ~Granstream() {
        for (int i = 0; i < chans; i++) {
            states[i].finish();
        }
        states.finish();
    }
    
    
    const char *classname() { return Granstream_name; }
    

    void print_details(int indent) {
        arco_print("polyphony %d dur %g enable %s fbdelay %g",
                   polyphony, dur, enable ? "true" : "false", feedback_delay);
        indent_spaces(indent + 2);
        arco_print("highdur %g lowdur %g density %g attack %g release %g",
                   highdur, lowdur, density, attack, release);
        indent_spaces(indent + 2);
        arco_print("feedback %g", feedback);
    }

    
    void print_sources(int indent, bool print_flag) {
        input->print_tree(indent, print_flag, "input");
    }


    void reset_gens() {  // initialize generators
        // set delay = 1 so that other parameters will be computed
        // on the next call to real_run():
        for (int i = 0; i < states.size(); i++) {
            states[i].reset_gens(0, polyphony);
        }
    }
    
    
    void init_input(Ugen_ptr ugen) { init_param(ugen, input, input_stride); }

    
    void repl_input(Ugen_ptr ugen) {
        input->unref();
        init_input(ugen);
    }


    void set_polyphony(int newp) {
        newp = MAX(0, newp);
        if (newp > polyphony) {
            for (int i = 0; i < states.size(); i++) {
                states[i].set_polyphony(polyphony, newp);
            }
        }
        polyphony = newp;
    }

    void set_delay(float d) {
        feedback_delay = MAX(BP, d);  // must delay at least one block
        int len = round(feedback_delay * AR);
        if (len > feedback_buf.get_fifo_len()) {  // (code from delay.h)
            feedback_buf.set_fifo_len(len, true);
            assert(feedback_buf.get_fifo_len() == len);
        }
    }


    void set_feedback(float f) {
        if (f > 0) {      // turning feedback on
            feedback = f;
            if (feedback_delay == 0) {  // initial value must be corrected
                feedback_delay = 1.0;
            }
            set_delay(feedback_delay);
        } else {
            feedback = 0.0;
        }
    }


    void set_gain(float g) {
        gain = g;
    }


    void set_dur(float d) {
        dur = d;
        // because grains must be extracted from within the buffer, and the
        // buffer is filled in blocks, we need not only room for a grain but
        // some padding at the beginning and ending to make sure the grain
        // samples are available in the buffer for the duration of the grain.
        // The minimum is 2*BP for padding + 20 msec for a (very) short grain:
        if (dur < 2 * BP + 0.02) {
            dur = 2 * BP + 0.02;
        }
        int len = ((int) (dur * AR) + BL) & ~(BL - 1);  // round up to BL
        for (int i = 0; i < states.size(); i++) {
            states[i].set_dur(len);
        }
    }
    
    // to disable, we set stop_request which is stops further grain production
    // and resets enable when no more grains are active. To enable, just
    // cancel stop_request and set enable to true.  If you set_enable(false),
    // change some parameters, but then set_enable(true) before the last grain
    // has finished, the individual Grain_gen's will pick up where they left
    // off and may not exactly follow parameter changes since the time of the
    // next grain start is already determined.
    void set_enable(bool enab) {
        stop_request = !enab;
        if (enab) {
            enable = true;
            stop_request = false;
        }
    }

    bool chan_a(Granstream_state *state, int chan) {
        return state->chan_a(this, out_samps, chan);
        
    }

    void real_run() {
        input_samps = input->run(current_block);
        Granstream_state *state = &states[0];
        bool active = false;
        // clear all output here because each channel (state) can sum grains
        // to each channel
        block_zero_n(out_samps, chans);
        
        for (int i = 0; i < states.size(); i++) {
            active |= chan_a(state, i);
            state++;
            input_samps += input_stride;
        }

        Sample fb[BL];
        block_zero(fb);
        // see if feedback is significant (above -90dB):
        if (feedback >= 0.00003 || feedback_in_smoothed >= 0.00003) {
            // sum all output channels, constructing feedback signal
            for (int chan = 0; chan < states.size(); chan++) {
                for (int i = 0; i < BL; i++) {
                    fb[i] += *out_samps++;
                }
            }

            // here, we compute feedback signal in fb[]. This signal
            // is processed by a limiter, but the user-controlled
            // feedback gain is applied on the output side of the
            // delay to give fast response to changes. Also, when user
            // sets feedback to zero, we smoothly attenuate the
            // feedback to zero and when the attenuation
            // (feedback_in_smoothed) is < -90dB, we stop the feedback
            // computation entirely. Feedback is smoothly ramped back
            // up when feedback becomes non-zero.
            //
            // The peak detector takes the max of the signal and the
            // "previous peak" (peak_envelope) to update peak_envelope
            // and low-passes with a 2ms rise time to get
            // peek_smoothed on every sample. Since this would result
            // in peak_envelope never decreasing, we move it toward 1
            // using a 1 sec time constant. Because of
            // fmax(peak_envelope, fabs(fb[i])), peak_envelope never
            // goes below 1. It rapidly moves up to peaks above 1 and
            // slowly recedes to 1 when peaks are below 1.
            float fbis_target = (feedback > 0);  // feedback_in_smoothed target

            // apply limiter and smoothly ramp on/off following fbis_target
            for (int i = 0; i < BL; i++) {
                peak_envelope = fmax(peak_envelope, fabs(fb[i]));
                peak_smoothed = peak_envelope +
                        risefactor * (peak_smoothed - peak_envelope);
                feedback_in_smoothed = fbis_target +
                        feedbackfactor * (feedback_in_smoothed - fbis_target);
                fb[i] *= feedback_in_smoothed / peak_smoothed;
                peak_envelope = 1 + fallfactor * (peak_envelope - 1);
            }
            D printf("peak %g peaksm %g fbenv %g fbgain %g\n", peak_envelope,
                   peak_smoothed, feedback_in_smoothed,
                   feedback_in_smoothed / peak_smoothed);
        }
        if (feedback_delay > 0) {
            feedback_buf.enqueue_block(fb);
        }
        // printf("peak_smoothed %g peak_envelope %g\n",
        //        peak_smoothed, peak_envelope);

        if (stop_request && !active) {
            stop_request = false;
            enable = false;
            reset_gens();  // prepare for enable
        }
    }
};
