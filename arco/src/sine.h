/* Sine.h -- unit generator for sine tone with frequency and amplitude
 *
 * Roger B. Dannenberg
 * Jan 2022
 */

class Sine : public Ugen {
public:
    Ugen_ptr freq;
    int freq_stride;
    Sample_ptr freq_samps;
    
    Ugen_ptr amp;
    int amp_stride;
    Sample_ptr amp_samps;

    Sine(int id, int nchans, int freq, int amp) : Ugen(id, 'a', nchans) { 
        init_freq(ugen_table[freq]);
        init_amp(ugen_table[amp]);
    };

    ~Sine() { freq->unref(); amp->unref(); }

    const char *name() { return "Sine"; }

    void print_sources(int indent, bool print) {
        freq->print_tree(indent, print, "freq");
        amp->print_tree(indent, print, "amp");
    }

    void repl_freq(Ugen_ptr inp) {
        freq->unref();
        init_amp(freq);
    }

    void repl_amp(Ugen_ptr inp) {
        amp->unref();
        init_amp(amp);
    }

    void set_freq(int chan, float f) {
        assert(freq->rate == 'c');
        freq->output[chan] = f;
    }

    void set_amp(int chan, float a) {
        assert(amp->rate == 'c');
        amp->output[chan] = a;
    }

    void init_amp(Ugen_ptr amp_) { init_param(amp_, amp, amp_stride); }

    void init_freq(Ugen_ptr freq_) { init_param(freq_, freq, freq_stride); }

    void chan_a_a(Sine_state *state) {
        for (int i = 0; i < BL; i++) {
            state->phase += *freq_samps * AR_RECIP;
            if (state->phase > 1) state->phase -= 1;
            else if (state->phase < 1) state->phase += 1;
            freq_samps++;
            *out_samps++ = sine_table[state->phase];
        }
    }

    void real_run() {
        freq_samps = freq->run(block_count);  // updat inputs
        amp_samps = amp->run(block_count);
        Sine_state *state = &states[0];
        for (int i = 0; i < chans; i++) {
            (this->*run_channel)(state);
            state++;
            freq_samps += freq_stride;
            amp_samps += amp_stride;
        }
    }
};
