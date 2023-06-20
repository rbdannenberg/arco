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
 */


enum Gran_state {GS_PREDELAY, GS_RISE, GS_HOLD, GS_FALL};

// a Gran_gen manages a single grain
class Gran_gen {
public:
    Gran_state state;
    int delay;           // blocks until next state change
    int release_blocks;  // how many blocks in envelope release
    int tostop_blocks;   // how many blocks to envelope gate down
    
    int dur_blocks; // how long is the grain in blocks?
    float ratio;  // sample rate conversion: higher ratio -> higher pitch
                  // ratio is uniform random between high and low
    float phase;  // buffer offset relative to now (negative)
    float attack_blocks;   // attack time in seconds
    float release_blokcs;  // decay time in seconds
    float env_val;  // current envelope value
    float env_inc;  // increment to next envelope value

    void reset() {
        state = GS_FALL;
        delay = 1;  // so next time we run, we'll reinitialize
    }

    void run(Granstream *gs, Granstream_state *state,
             Sample_ptr out_samps, int i) {
        if (--delay == 0) {
            switch (state) {
            case GS_FALL: {  // now at end of envelope
                if (!granstream->enable) {
                    return;  // do not start another grain; we're done
                }
                dur_blocks = (int) (unifrand_range(gs->lowdur, gs->highdur) *
                                    AR + 0.5);
                attack_blocks = (int) (gs->attack * VR + 1);
                if (attack_blocks < 0) {
                    attack_blocks = 1;
                }
                release_blocks = (int) gs->release * VR + 1);
                if (release_blocks < 0) {
                    release_blocks = 1;
                }
                if (attack_blocks + release_blocks > dur_blocks) {
                    dur_blocks = attack_blocks + release_blocks;
                }
                ratio = unifrand_range(gs->low, gs->high);
                // avgdur tries to factor out the attack/release
                // to get "effective avgdur"; assume effective duration
                // includes only half of attack and duration
                float avgdur = gs->lowdur + gs->highdur -
                               (gs->attack + gs->release) * 0.5;
                float avgioi = avgdur * gs->polyphony / gs->density;
                // If all grains were the same duration (avgdur), we
                // could pick a delay from the end of one grain to the
                // beginning of the next grain to be between 0 and 2 *
                // (ioi - avgdur) (uniformly distributed) to obtain an
                // average ioi of avgioi.  Things are more complicated
                // when duration is not always avgdur, but we'll use
                // the actual dur of the present grain:
                float ts = 2 * (avgioi - avgdur) * unifrand();
                if (ts < 0) {
                    ts = 0;
                }
                delay = (int) (ts * BR);
                if (delay < 0) delay = 0;
                tostop = dur_blocks - release_blocks;
                state = GS_PREDELAY;
                if (delay > 0) {
                    break;
                }
            case GS_PREDELAY: {  // start of the grain computation
                if (!gs->enable) {
                    return;
                }
                // phase (here) is where we start in buffer relative to now
                phase = -gs->dur * AR * unifrand();
                // number of samples from input buffer is increment per
                // output sample * number of output samples
                float dur_in_samples = dur_blocks * BL;
                // To avoid underflow, check the following 
                // (see notes at top)
                // (another note: add 2 to allow interpolation 
                // and rounding):
                double final_phase = phase + (dur_in_samples + 2) * (ratio - 1);
                if (final_phase < BL - gs->bufferlen) {
                    // 1 sample fudge factor:
                    float advance = (BL - gs->bufferlen) - final_phase - 1;
                    final_phase += advance;
                    phase += advance;
                } else if (final_phase > -1) {
                    phase -= (final_phase + 1); // 1 sample fudge factor
                    final_phase = -1;
                }
                // it may be that we can't satisfy both constraints, 
                // so check again. Also, make sure phase is initially 
                // in buffer. Only play grain if it is possible:
                if (phase > BL - gs->bufferlen && 
                    final_phase > BL - gs->bufferlen && 
                    phase < 0 && final_phase < 0) {
                    // start the note
                    env_val = 0;
                    env_inc = 1.0 / (attack_blocks * BL);
                    // note: phase is negative; map phase to buffer offset
                    // in case ratio is 1, avoid interpolation
                    phase = (int) (state->tail - VL + phase);
                    if (phase < 0) phase += state->samps.size();
                    state = GS_RISE;
                    delay = attack_blocks;
                    assert(phase >= 0 && phase < state->samps.size());
                } else {
                    delay = 1; // try again next block
                    state = GS_PREDELAY;
                    // only one message per 1000 blocks to avoid flood of errors
                    if (gs->warningblock > gs->blocksdone) {
                        printf("igranstr grain too long\n");
                        gs->warningblock = gs->blocksdone + 1000;
                    }
                }
                break;
            }
            case GS_HOLD: {
                delay = dur_blocks - attack_blocks - release_blocks;
                state = GS_FALL;
                env_val = 1;
                env_inc = 0;
                if (delay > 0) {
                    break;
                } // otherwise begin fall immediately
            }
            case GS_FALL: {
                delay = release_blocks;
                env_val = 0;
                env_inc = 1.0 / (release_blocks * BL);
                break;
            }
            }
            if (state != GS_PREDELAY) {  // envelope is non-zero
                assert(state->samps.bounds_check((int) phase));
                Sample ampl;
                int bufferlen = state->samps.size();
                // non-standard output: Instead of each channel (state)
                // generating one channel of output, in this Ugen, grains
                // are spread across all output channels equally. Note
                // that out_samps is a parameter, so we are not updating
                // an instance variable, and neither does real_run().
                YOU ARE HERE
                for (int i = 0; i < BL; i++) {
                    int index = (int) phase;
                    int index2 = index + 1;
                    if (index2 >= bufferlen) {
                        index2 = 0;
                    }
                    Sample x1 = state->samps[index];
                    Sample x2 = state->samps[index2];
                    Sample x = x1 + (phase - index) * (x2 - x1);
                    env_val += env_inc;
                    *out_samps++ += x * env_val;
                    phase += ratio;
                    if (phase >= bufferlen) {
                        phase -= bufferlen;
                    }
                }
    }
};

typedef Gran_gen *Gran_gen_ptr;

class Granstream : public Ugen {
    friend Gran_gen;
public:
    struct Granstream_state {  // information for a single channel
        Vec<float> samps;  // delay line
        int tail;  // tail indexes the *next* location to put input
        // tail also indexes the oldest input, which has a delay of
        // samps.size() samples
        Vec<Gran_gen> gens;  // individual grains

        void init(float dur, int polyphony) {
            int len = ((int) (dur * AR) + BL) & ~(BL - 1);  // round up to BL
            samps.init(len);
            samps.set_size(samps.get_allocated());
            samps.zero();
            tail = 0;
            gens.init(polyphony);
        }

        void reset_gens(int polyphony) {
            for (int i = 0; i < polyphony; i++) {
                gens[i].reset();
            }
        }

        void set_polyphony(int polyphony) {
            samps.set_size(polyphony);
            rest_gens(polyphony);
        }

        void set_dur(int len) {
            if (len > samps.size()) {  // see delay.h for algorithm notes
                int old_length = samps.size();
                int grow = len - old_length;
                Sample_ptr ptr = samps.append_space(grow);
                int m = (tail == 0 ? 0 : old_length - tail);
                memmove(&samps[tail + grow], &samps[tail],
                        grow * sizeof(Sample));
                memset(&samps[tail], 0, grow * sizeof(Sample));
            }
        }
    };
    
    int polyphony;
    float dur;
    bool enable;
    Vec<Granstream_state> states;

    Ugen_ptr inp;
    int inp_stride;
    Sample_ptr inp_samps;
    
    void Granstream(int id, int nchans, Ugen_ptr inp_, int polyphony_,
                    float dur_, bool enable_) : Ugen(id, 'a', nchans) {
    /* polyphony - number of grains per channel
     * dur - the duration (s) of the sample buffer per channel
     */
        inp = inp_;
        polyphony = polyphony_;
        dur = dur_;
        enable = enable_;
        states.init(chans);
        init_gain(gain);
        for (int i = 0; i < chans; i++) {
            states[0].init(polyphony);
        }
        set_gate(gate);
        initialized = true;
    }

    
    void reset_gens() {  // initialize generators
        // set delay = 1 so that other parameters will be computed
        // on the next call to real_run():
        for (int i = 0; i < states.size(); i++) {
            states[i].reset_gens(polyphony);
        }
    }

    
    void repl_inp(Ugen_ptr ugen) {
        inp->unref();
        init_inp(ugen);
    }


    void set_polyphony(int p) {
        polyphony = p;
        for (int i = 0; i < states.size(); i++) {
            states[i].set_polyphony(p);
        }
        reset_gens();
    }

    
    void set_dur(float d) {
        dur = d;
        int len = ((int) (dur * AR) + BL) & ~(BL - 1);  // round up to BL
        for (int i = 0; i < states.size(); i++) {
            states[i].set_dur(len);
        }
    }

    void chan_a(Granstream_state *state) {
        // first, read input audio into input buffer
        memcpy(&state->samps[tail], inp_samps, BL * sizeof(Sample));
        tail += BL;
        if (tail >= state->samps.size()) {
            tail = 0;
        }
        memset(out_samps, 0, BL * sizeof(Sample));
        for (int i = 0; i < polyphony; i++) {
            gens[i].run(this, state, out_samps, i);
        }
    }

    void real_run() {
        inp_samps = inp->run(current_block);
        Granstream_state *state = &states[0];
        for (int i = 0; i < states.size(); i++) {
            chan_a(state);
            state++;
            inp_samps += inp_stride;
        }
    }
        

    // high - the upper limit on ratio
    double high;  /*A get set A*/
    // low - the lower limit on ratio
    double low;   /*A get set A*/
    // highdur - the upper limit on grain duration
    double highdur; /*A get set A*/
    // lowdur - the lower limit on grain duration
    double lowdur;  /*A get set A*/
    double delay;  /*A get set A*/
    double density; /*A get set A*/
    void set_gate(double newgate); /*A visible A*/
    double gate; /*A get A*/
    float attack; /*A get set A*/
    float release; /*A get set A*/
    // the length of the sample buffer from which grains are taken:
    void set_length(double length); /*A visible A*/
    int bufferlen; // buffer length in samples
    // the number of parallel grain generators:
 
protected:

    Uadd_aa_a add;

    long warningblock;
    bool stopped;
    bool request_stop;

    sample *buffer;
    int next_input; // offset in buffer for next input samples
    Gran_gen_ptr gens;

    sample *in_ptr;
    sample *gran_out;
    virtual void real_run(long newblock);

public:
    void msg_handler(Aura_msg *, uint32 len, double t);
    Igranstr();
    virtual void init_io();

    /* aura-preproc */
    #include "igranstr.ah"
    /* end-preproc */
};

/* aura-preproc */
extern Aura_dict igranstr_aura_dict;
/* end-preproc */
