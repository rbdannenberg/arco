/* tableosc.h -- simple table-lookup oscillator
 *
 * Roger B. Dannenberg
 * Oct 2024
 */

extern const char *Tableosc_name;

class Tableosc : public Wavetables {
public:
    struct Tableosc_state {
        double phase;
        Sample prev_amp;
    };
    int which_table;
    double phase_scale;
    Vec<Tableosc_state> states;
    void (Tableosc::*run_channel)(Tableosc_state *state,
                                  Sample *table, int tlen);


    Ugen_ptr freq;
    int freq_stride;
    Sample_ptr freq_samps;

    Ugen_ptr amp;
    int amp_stride;
    Sample_ptr amp_samps;


    Tableosc(int id, int nchans, Ugen_ptr freq_, Ugen_ptr amp_) : 
            Wavetables(id, nchans) {
        which_table = 0;
        states.set_size(chans);
        for (int i = 0; i < chans; i++) {
            states[i].phase = 0;
        }
        init_freq(freq_);
        init_amp(amp_);
        update_run_channel();
    }

    ~Tableosc() {
        freq->unref();
    }

    const char *classname() { return Tableosc_name; }

    void update_run_channel() {
        if (freq->rate == 'a') {
            if (amp->rate == 'a') {
                run_channel = &Tableosc::chan_aa_a;
            } else {
                run_channel = &Tableosc::chan_ab_a;
            }
        } else {
            if (amp->rate == 'a') {
                run_channel = &Tableosc::chan_ba_a;
            } else {
                run_channel = &Tableosc::chan_bb_a;
            }
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
        freq->const_set(chan, f, "Tableosc::set_freq");
    }

    void init_freq(Ugen_ptr ugen) { init_param(ugen, freq, &freq_stride); }

    void repl_amp(Ugen_ptr ugen) {
        amp->unref();
        init_amp(ugen);
        update_run_channel();
    }

    void set_amp(int chan, float f) {
        amp->const_set(chan, f, "Tableosc::set_amp");
    }

    void init_amp(Ugen_ptr ugen) { init_param(ugen, amp, &amp_stride); }


    void select(int i) {  // select table
        if (i < 0 || i >= num_tables()) {
            i = 0;
        }
        which_table = i;
        Wavetable *table = get_table(i);
        if (table) {
            phase_scale = (table->size() - 2) * AP;
        }
    }


    void chan_aa_a(Tableosc_state *state, Sample *table, int tlen) {
        double phase = state->phase;
        for (int i = 0; i < BL; i++) {
            int iphase = phase;
            float frac = phase - iphase;
            *out_samps++ = (table[iphase] * (1 - frac) + 
                            table[iphase + 1] * frac) * amp_samps[i];
            phase += freq_samps[i] * phase_scale;
            while (phase > tlen) phase -= tlen;
            while (phase < 0) phase += tlen;
        }
    }


    void chan_ba_a(Tableosc_state *state, Sample *table, int tlen) {
        double phase = state->phase;
        for (int i = 0; i < BL; i++) {
            int iphase = phase;
            float frac = phase - iphase;
            *out_samps++ = (table[iphase] * (1 - frac) + 
                            table[iphase + 1] * frac) * amp_samps[i];
            phase += *freq_samps * phase_scale;
            while (phase > tlen) phase -= tlen;
            while (phase < 0) phase += tlen;
        }
        state->phase = phase;
    }


    void chan_ab_a(Tableosc_state *state, Sample *table, int tlen) {
        double phase = state->phase;
        Sample amp_sig = *amp_samps;
        Sample amp_sig_fast = state->prev_amp;
        state->prev_amp = amp_sig;
        for (int i = 0; i < BL; i++) {
            int iphase = phase;
            float frac = phase - iphase;
            *out_samps++ = (table[iphase] * (1 - frac) + 
                            table[iphase + 1] * frac) * amp_sig_fast;
            phase += freq_samps[i] * phase_scale;
            while (phase > tlen) phase -= tlen;
            while (phase < 0) phase += tlen;
        }
    }


    void chan_bb_a(Tableosc_state *state, Sample *table, int tlen) {
        double phase = state->phase;
        Sample amp_sig = *amp_samps;
        Sample amp_sig_fast = state->prev_amp;
        state->prev_amp = amp_sig;
        Sample amp_sig_incr = (amp_sig - amp_sig_fast) * BL_RECIP;
        for (int i = 0; i < BL; i++) {
            int iphase = phase;
            float frac = phase - iphase;
            amp_sig_fast += amp_sig_incr;
            *out_samps++ = (table[iphase] * (1 - frac) + 
                            table[iphase + 1] * frac) * amp_sig_fast;
            phase += *freq_samps * phase_scale;
            while (phase > tlen) phase -= tlen;
            while (phase < 0) phase += tlen;
        }
        state->phase = phase;
    }


    void real_run() {
        freq_samps = freq->run(current_block); // update input
        amp_samps = amp->run(current_block); // update input
        Tableosc_state *state = &states[0];
        if (which_table >= num_tables()) {
            return;
        }
        Wavetable *table = get_table(which_table);
        if (!table) {
            return;
        }
        int tlen = table->size() - 2;
        if (tlen < 2) {
            return;
        }
        for (int i = 0; i < chans; i++) {
            (this->*run_channel)(state, &((*table)[0]), tlen);
            state++;
            freq_samps += freq_stride;
            amp_samps += amp_stride;
        }
    }
};
