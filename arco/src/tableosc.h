/* tableosc.h -- simple table-lookup oscillator
 *
 * Roger B. Dannenberg
 * Oct 2024
 */

extern const char *Tableosc_name;

const int64_t float_to_fix = (int64_t) 1 << 32;
const float fix_to_float = 1.0f / float_to_fix;
const float freq_to_phase_incr = AP * float_to_fix;

class Tableosc : public Wavetables {
public:
    struct Tableosc_state {
        int64_t phase;  // fixed point from 0 to 1 with 32 fractional bits
        Sample prev_amp;
    };
    int which_table;
    Vec<Tableosc_state> states;
    void (Tableosc::*run_channel)(Tableosc_state *state,
                                  Sample *table, int tlen);


    Ugen_ptr freq;
    int freq_stride;
    Sample_ptr freq_samps;

    Ugen_ptr amp;
    int amp_stride;
    Sample_ptr amp_samps;


    Tableosc(int id, int nchans, Ugen_ptr freq_, Ugen_ptr amp_, float phase) :
            Wavetables(id, 'a', nchans) {
        which_table = 0;
        states.set_size(chans);
        Wavetable *table = get_table(which_table);
        int tlen = 0;
        if (table) {
            tlen = table->size() - 2;
        }
        // if there is no table yet, phase will be initialized to zero.
        // otherwise, we initialize phase according to the phase parameter,
        // and ASSUMING that table[0] has the right table length, even if
        // a different table is selected before playing starts.
        int64_t phase64 = fmodf(phase / 360.0f, 1.0f) * float_to_fix * tlen;
        for (int i = 0; i < chans; i++) {
            states[i].phase = phase64;
            states[i].prev_amp = 0.0f;
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

    void set_phase(int chan, float f) {
        Wavetable *table = get_table(which_table);
        int tlen = 0;
        if (table) {
            tlen = table->size() - 2;
        }
        int64_t phase64 = fmodf(f / 360.0f, 1.0f) * float_to_fix * tlen;
        states[chan].phase = phase64;
    }

    void init_amp(Ugen_ptr ugen) { init_param(ugen, amp, &amp_stride); }


    // Note: selecting a new table will maintain the phase ONLY if the
    // selected table has the same size as the current table since phase
    // is maintained as a fractional table INDEX.  If the new table is
    // shorter than the current table, the phase (index) will be wrapped
    // into a valid index if necessary (see the inner loops).
    void select(int i) {  // select table
        if (i < 0 || i >= num_tables()) {
            i = 0;
        }
        which_table = i;
    }


    void chan_aa_a(Tableosc_state *state, Sample *table, int tlen) {
        int64_t phase = state->phase;
        int64_t tlen_mask = ((int64_t) tlen << 32) - 1;
        float freq_scale = freq_to_phase_incr * tlen;
        for (int i = 0; i < BL; i++) {
            // doing this first allows us to change tables, possibly changing
            // table size, without worrying about phase overflow:
            phase &= tlen_mask;
            int ix = phase >> 32;
            float frac = phase & 0xFFFFFFFF;
            *out_samps++ = (table[ix] * (0x100000000 - frac) + 
                            table[ix + 1] * frac) * amp_samps[i] * fix_to_float;
            phase += freq_samps[i] * freq_scale;
        }
        state->phase = phase;
    }


    void chan_ba_a(Tableosc_state *state, Sample *table, int tlen) {
        int64_t phase = state->phase;
        int64_t tlen_mask = ((int64_t) tlen << 32) - 1;
        double phase_incr = *freq_samps * freq_to_phase_incr * tlen;
        for (int i = 0; i < BL; i++) {
            // doing this first allows us to change tables, possibly changing
            // table size, without worrying about phase overflow:
            phase &= tlen_mask;
            int ix = phase >> 32;
            float frac = phase & 0xFFFFFFFF;
            *out_samps++ = (table[ix] * (0x100000000 - frac) + 
                            table[ix + 1] * frac) * amp_samps[i] * fix_to_float;
            phase += phase_incr;
        }
        state->phase = phase;
    }

    void chan_ab_a(Tableosc_state *state, Sample *table, int tlen) {
        int64_t phase = state->phase;
        int64_t tlen_mask = ((int64_t) tlen << 32) - 1;
        float freq_scale = freq_to_phase_incr * tlen;
        Sample amp_sig = *amp_samps * fix_to_float;
        Sample amp_sig_fast = state->prev_amp;
        state->prev_amp = amp_sig;
        Sample amp_sig_incr = (amp_sig - amp_sig_fast) * BL_RECIP;
        for (int i = 0; i < BL; i++) {
            // doing this first allows us to change tables, possibly changing
            // table size, without worrying about phase overflow:
            phase &= tlen_mask;
            int ix = phase >> 32;
            float frac = phase & 0xFFFFFFFF;
            amp_sig_fast += amp_sig_incr;
            *out_samps++ = (table[ix] * (0x100000000 - frac) + 
                            table[ix + 1] * frac) * amp_sig_fast;
            phase += freq_samps[i] * freq_scale;
        }
        state->phase = phase;
    }


    void chan_bb_a(Tableosc_state *state, Sample *table, int tlen) {
        int64_t phase = state->phase;
        int64_t tlen_mask = ((int64_t) tlen << 32) - 1;
        double phase_incr = *freq_samps * freq_to_phase_incr * tlen;
        Sample amp_sig = *amp_samps * fix_to_float;
        Sample amp_sig_fast = state->prev_amp;
        state->prev_amp = amp_sig;
        Sample amp_sig_incr = (amp_sig - amp_sig_fast) * BL_RECIP;
        for (int i = 0; i < BL; i++) {
            // doing this first allows us to change tables, possibly changing
            // table size, without worrying about phase overflow:
            phase &= tlen_mask;
            int ix = phase >> 32;
            float frac = phase & 0xFFFFFFFF;
            amp_sig_fast += amp_sig_incr;
            *out_samps++ = (table[ix] * (0x100000000 - frac) + 
                            table[ix + 1] * frac) * amp_sig_fast;
            phase += phase_incr;
        }
        state->phase = phase;
    }


    void real_run() {
        freq_samps = freq->run(current_block); // update input
        amp_samps = amp->run(current_block); // update input
        Tableosc_state *state = &states[0];
        Wavetable *table = get_table(which_table);
        if (!table) {
            return;
        }
        int tlen = table->size() - 2;
        assert(((tlen - 1) & tlen) == 0);
        if (tlen < 2) {
            return;
        }
        for (int i = 0; i < chans; i++) {
            (this->*run_channel)(state, table->get_array(), tlen);
            state++;
            freq_samps += freq_stride;
            amp_samps += amp_stride;
        }
    }
};
