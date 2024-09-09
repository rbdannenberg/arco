/* feedback.h -- unit generator that passes audio through
 *
 * Roger B. Dannenberg
 * Jan 2022
 */

extern const char *Feedback_name;

class Feedback : public Ugen {
  public:
    struct Feedback_state {
        Sample gain_prev;  // used for interpolation
    };
    Vec<Feedback_state> states;
    Vec<Sample> feedback;
    void (Feedback::*run_channel)(Feedback_state *state);
    
    Ugen_ptr input;
    int input_stride;
    Sample_ptr input_samps;

    Ugen_ptr gain;
    int gain_stride;
    Sample_ptr gain_samps;
    
    Ugen_ptr from;  // feedback is comes from here
    int from_stride;
    Sample_ptr from_samps;

    Feedback(int id, int nchans, Ugen_ptr input, Ugen_ptr from, Ugen_ptr gain) :
            Ugen(id, 'a', nchans) {
        states.set_size(chans);
        init_input(input);
        init_from(from);
        init_gain(gain);
        run_channel = (void (Feedback::*)(Feedback_state *)) 0;
        update_run_channel();
    }

    ~Feedback() {
        input->unref();
        gain->unref();
        from->unref();  // actually, if from is referenced, then it probably
        // forms a cycle, so we cannot be deconstructed. Normally, you have
        // to break the cycle by setting the feedback to the zero Ugen, which
        // never gets freed, so we don't really have to unref it; however, it
        // is possible that from is not downstream and there's no cycle, so
        // we really want to unref it here to maintain reference counting.
    }

    const char *classname() { return Feedback_name; }

    void initialize_channel_states() {
        for (int i = 0; i < chans; i++) {
            states[i].gain_prev = 0.0f;
        }
    }


    void update_run_channel() {
        // initialize run_channel based on input types
        void (Feedback::*new_run_channel)(Feedback_state *state);
        if (gain->rate == 'a') {
            new_run_channel = &Feedback::chan_aa_a;
        } else {
            new_run_channel = &Feedback::chan_ab_a;
        }
        if (new_run_channel != run_channel) {
            initialize_channel_states();
            run_channel = new_run_channel;
        }
    }

    void print_sources(int indent, bool print_flag) {
        input->print_tree(indent, print_flag, "input");
        from->print_tree(indent, print_flag, "from");
        gain->print_tree(indent, print_flag, "gain");
    }

    void repl_input(Ugen_ptr ugen) {
        input->unref();
        init_input(ugen);
    }

    void repl_from(Ugen_ptr f) {
        from->unref();
        init_from(f);
        feedback.init(from->chans * BL, true);
    }

    void repl_gain(Ugen_ptr g) {
        gain->unref();
        init_gain(g);
        update_run_channel();
    }

    void set_gain(int chan, float g) {
        gain->const_set(chan, g, "Feedback::set_gain");
    }

    void init_input(Ugen_ptr ugen) { init_param(ugen, input, &input_stride); }

    void init_from(Ugen_ptr ugen) { init_param(ugen, from, &from_stride); }

    void init_gain(Ugen_ptr ugen) { init_param(ugen, gain, &gain_stride); }

    void chan_aa_a(Feedback_state *state) {
        for (int i = 0; i < BL; i++) {
            *out_samps++ = input_samps[i] + from_samps[i] * gain_samps[i];
        }
    }

    void chan_ab_a(Feedback_state *state) {
        Sample gain_inc = (*gain_samps - state->gain_prev) * BL_RECIP;
        Sample gain_fast = state->gain_prev;
        state->gain_prev = *gain_samps;
        for (int i = 0; i < BL; i++) {
            gain_fast += gain_inc;
            *out_samps++ = input_samps[i] + from_samps[i] * gain_fast;
        }
    }

    void real_run() {
        input_samps = input->run(current_block);  // update input
        from_samps = &feedback[0];  // use previous block
        gain_samps = gain->run(current_block);  // update gain
        Feedback_state *state = &states[0];
        for (int i = 0; i < chans; i++) {
            (this->*run_channel)(state);
            state++;
            input_samps += input_stride;
            from_samps += from_stride;
            gain_samps += gain_stride;
        }
        // tricky: we could not update "from" earlier because that would
        // create a cycle of dependencies and infinite recursion, but now
        // we get samples from "from" to be used as feedback to the next
        // call to real_run():
        from_samps = from->run(current_block);
        block_copy_n(&feedback[0], from_samps, from->chans);
    }
};
