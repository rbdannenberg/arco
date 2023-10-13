/* upsample -- unit generator for arco
 *
 * Roger B. Dannenberg
 * Jan 2022
 */

extern const char *Upsample_name;

class Upsample : public Ugen {
public:
    struct Upsample_state {
        float input_prev;
    };
    Vec<Upsample_state> states;

    Ugen_ptr input;
    int32_t input_stride;

    Upsample(int id, int nchans, Ugen_ptr input_) : Ugen(id, 'a', nchans) {
        input = input_;
        states.set_size(chans);

        // initialize channel states
        for (int i = 0; i < chans; i++) {
            states[i].input_prev = 0.0f;
        }
        init_input(input);
    }

    ~Upsample() {
        input->unref();
    }

    const char *classname() { return Upsample_name; }

    void print_sources(int indent, bool print_flag) {
        input->print_tree(indent, print_flag, "input");
    }


    void init_input(Ugen_ptr ugen) { init_param(ugen, input, input_stride); }
    

    void repl_input(Ugen_ptr input) {
        input->unref();
        init_input(input);
    }


    void set_input(int chan, Sample x) {
        input->const_set(chan, x, "Upsample::set_input");
    }


    void real_run() {
        Sample_ptr input_samps = input->run(current_block); // update input
        Upsample_state *state = &states[0];
        for (int i = 0; i < chans; i++) {
            float input_incr = (*input_samps - state->input_prev) * BL_RECIP;
            for (int j = 0; j < BL; j++) {
                state->input_prev += input_incr;
                *out_samps++ = state->input_prev;
            }
            state++;
            input_samps += input_stride;
        }
    }
};
