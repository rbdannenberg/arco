/* tableoscb -- simple table-lookup oscillator
 *
 * Roger B. Dannenberg
 * Oct 2024
 */

extern const char *Tableoscb_name;

class Tableoscb : public Wavetables {
public:
    struct Tableoscb_state {
        double phase;
    };
    double phase_scale;
    int which_table;
    Sample *table;
    Vec<Tableoscb_state> states;

    Ugen_ptr freq;
    int freq_stride;
    Sample_ptr freq_samps;

    Ugen_ptr amp;
    int amp_stride;
    Sample_ptr amp_samps;


    Tableoscb(int id, int nchans, Ugen_ptr freq_, Ugen_ptr amp_, float phase) :
            Wavetables(id, 'b', nchans) {
        which_table = 0;
        states.set_size(chans);
        for (int i = 0; i < chans; i++) {
            states[i].phase = fmodf(phase / 360.0f, 1.0f);
        }
        init_freq(freq_);
        init_amp(amp_);
        update_run_channel();
    }

    ~Tableoscb() {
        freq->unref();
    }

    const char *classname() { return Tableoscb_name; }

    void update_run_channel() {
        if (freq->rate == 'a') {
            freq = new Dnsampleb(-1, freq->chans, freq, LOWPASS500);
        }
        if (amp->rate == 'a') {
            amp = new Dnsampleb(-1, amp->chans, amp, LOWPASS500);
        }
    }

    void print_sources(int indent, bool print_flag) {
        freq->print_tree(indent, print_flag, "freq");
        amp->print_tree(indent, print_flag, "amp");
    }

    void repl_freq(Ugen_ptr ugen) {
        freq->unref();
        init_freq(ugen);
        update_run_channel();
    }

    void set_freq(int chan, float f) {
        freq->const_set(chan, f, "Tableoscb::set_freq");
    }

    void init_freq(Ugen_ptr ugen) { init_param(ugen, freq, &freq_stride); }

    void repl_amp(Ugen_ptr ugen) {
        amp->unref();
        init_amp(ugen);
        update_run_channel();
    }

    void set_amp(int chan, float f) {
        amp->const_set(chan, f, "Tableoscb::set_amp");
    }

    void set_phase(int chan, float f) {
        states[chan].phase = fmodf(f / 360.0f, 1.0f);
    }

    void init_amp(Ugen_ptr ugen) { init_param(ugen, amp, &amp_stride); }


    void select(int i) {  // select table
        if (i >= 0 && i < wavetables.size()) {
            which_table = i;
        }
    }


    void real_run() {
        Sample_ptr freq_samps = freq->run(current_block); // update input
        Sample_ptr amp_samps = amp->run(current_block); // update input
        Tableoscb_state *state = &states[0];
        Wavetable *table = get_table(which_table);
        if (!table) {
            return;
        }
        int tlen = table->size() - 2;
        if (tlen < 2) {
            return;
        }
        float *waveform = table->get_array();
        for (int i = 0; i < chans; i++) {
            double phase = state->phase;
            float x = phase * tlen;
            int ix = x;
            float frac = x - ix;
            *out_samps++ = (waveform[ix] * (1 - frac) +
                            waveform[ix + 1] * frac) * *amp_samps;
            phase += *freq_samps * AP;
            while (phase > 1) phase--;
            while (phase < 0) phase++;
            state->phase = phase;
            state++;
            freq_samps += freq_stride;
            amp_samps += amp_stride;
       }
    }
};
