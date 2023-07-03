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

extern const char *Granstream_name;

class Granstream;
class Granstream_state;

enum Gran_state {GS_PREDELAY, GS_RISE, GS_HOLD, GS_FALL};

// a Gran_gen manages a single grain; there are polyphony of these per
// channel, stored in the per-channel state called Granstream_state;
class Gran_gen {
public:
    Gran_state state;    // state machine state (not the per-channel state)
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

    // returns true if grain is active
    bool run(Granstream *gs, Granstream_state *chaninfo,
             Sample_ptr out_samps, int i);
};


class Granstream_state {  // information for a single channel
public:
    Vec<float> samps;  // delay line
    int tail;  // tail indexes the *next* location to put input
    // tail also indexes the oldest input, which has a delay of
    // samps.size() samples
    Vec<Gran_gen> gens;  // individual grains

    void init(float dur, int polyphony) {
        int len = ((int) (dur * AR) + BL) & ~(BL - 1);  // round up to BL
        samps.init(len);
        samps.set_size(samps.get_allocated());  // zeros samps
        tail = 0;
        gens.init(polyphony);
        gens.set_size(polyphony, false);
        reset_gens(polyphony);
    }
    
    void finish() {
        samps.finish();
        gens.finish();
    }
    
    // returns true if any grain is active
    bool chan_a(Granstream *gs, Sample *out_samps);

    void reset_gens(int polyphony) {
        for (int i = 0; i < polyphony; i++) {
            gens[i].reset();
        }
    }

    void set_polyphony(int polyphony) {
        samps.set_size(polyphony);
        reset_gens(polyphony);
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



class Granstream : public Ugen {
    friend Gran_gen;
public:
    int polyphony;  // how many grains can we have at once per channel
    float dur;      // how far we can reach back in time to find a grain
    bool enable;    // start/stop making grains
    Vec<Granstream_state> states;

    Ugen_ptr inp;
    int inp_stride;
    Sample_ptr inp_samps;
    
    float high;     // the upper limit on ratio
    float low;      // the lower limit on ratio
    float highdur;  // the upper limit on grain duration
    float lowdur;   // the lower limit on grain duration
    float density;  // average number of active grains/input channel
    float attack;   // the grain attack time in seconds
    float release;  // the grain release time in seconds
    int bufferlen;  // buffer length in samples (based on dur)
    int warning_block;  // used to limit rate of warning messages
    bool stop_request;  // waiting for last grain to finish before setting
                        // enable to false.
    
    Granstream(int id, int nchans, Ugen_ptr inp_, int polyphony_,
                    float dur_, bool enable_) : Ugen(id, 'a', nchans) {
    /* polyphony - number of grains per channel
     * dur - the duration (s) of the sample buffer per channel
     */
        inp = inp_;
        polyphony = polyphony_;
        dur = dur_;
        enable = enable_;

        attack = 0.02;
        release = 0.02;
        high = 1.0;
        low = 1.0;
        highdur = 0.1;
        lowdur = 0.1;
        density = polyphony * 0.5;
        warning_block = 0;
        stop_request = false;

        states.init(chans);
        states.set_size(chans, false);
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
    

    void reset_gens() {  // initialize generators
        // set delay = 1 so that other parameters will be computed
        // on the next call to real_run():
        for (int i = 0; i < states.size(); i++) {
            states[i].reset_gens(polyphony);
        }
    }
    
    
    void init_inp(Ugen_ptr ugen) { init_param(ugen, inp, inp_stride); }

    
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
        }
    }

    bool chan_a(Granstream_state *state) {
        return state->chan_a(this, out_samps);
        
    }

    void real_run() {
        inp_samps = inp->run(current_block);
        Granstream_state *state = &states[0];
        bool active = false;
        for (int i = 0; i < states.size(); i++) {
            active |= chan_a(state);
            state++;
            inp_samps += inp_stride;
        }
        if (stop_request && !active) {
            stop_request = false;
            enable = false;
            reset_gens();  // prepare for enable
        }
    }
};
