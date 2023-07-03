/* mix.h -- unit generator that passes audio through
 *
 * Roger B. Dannenberg
 * Feb 2022
 */

extern const char *Mix_name;

class Mix : public Ugen {
public:
    struct Input {
        Ugen_ptr input;
        Ugen_ptr gain;
        float prev_gain;
    };
    Vec<Input> inputs;
    

    Mix(int id, int nchans) : Ugen(id, 'a', nchans) {
    };

    ~Mix();

    const char *classname() { return Mix_name; }

    void print_sources(int indent, bool print);

    // insert operation takes a signal and a gain
    void ins(Ugen_ptr inp, Ugen_ptr gain) {
        if (inp->rate != 'a') {
            arco_warn("mix_ins: input rate is not 'a', ignore insert");
            return;
        }
        if (gain->rate == 'a') {
            arco_warn("mix_ins: gain rate must be 'b' or 'c', ignore insert");
            return;
        }
        if (gain->chans != 1) {
            arco_warn("mix_ins: gain chans > 1, using only channel 0");
            return;
        }
        Input *input = inputs.append_space(1);
        int ignore_stride;
        init_param(inp, input->input, ignore_stride);
        init_param(gain, input->gain, ignore_stride);
        input->prev_gain = 0.0f;
        // arco_print("after Mix::ins, inp->refcount %d\n", inp->refcount);
    }


    // find the index of Ugen inp -- linear search
    int find(Ugen_ptr inp) {
        //Input *input = &inputs[0]; This line looks like a bug!
        for (int i = 0; i < inputs.size(); i++) {
            Input *input = &inputs[i];
            if (input->input == inp) {
                return i;
            }
        }
        arco_warn("mix_rem: Ugen not found, nothing was changed");
        return -1;
    }
        

    // remove operation finds the signal and removes it and its gain
    void rem(Ugen_ptr inp) {
        int i = find(inp);
        if (i >= 0) {
            Input *input = &inputs[i];
            input->input->unref();
            input->gain->unref();
            inputs.remove(i);
        }
    }

    void set_gain(Ugen_ptr inp, float gain) {
        int i = find(inp);
        if (i >= 0) {
            Input *input = &inputs[i];
            Ugen *gain_ugen = input->gain;
            if (gain_ugen->rate == 'c') {
                gain_ugen->output[0] = gain;
            } else {
                gain_ugen->unref();
                input->gain = new Const(-1, 1, gain);
            }
        }
    }

    void repl_gain(Ugen_ptr inp, Ugen_ptr gain) {
        int i = find(inp);
        if (i >= 0) {
            Input *input = &inputs[i];
            Ugen *gain_ugen = input->gain;
            gain_ugen->unref();
            gain->ref();
            input->gain = gain;
        }
    }


    void real_run() {
        // zero the outputs
        memset(out_samps, 0, BLOCK_BYTES * chans);
        // add inputs with wrap-around
        Input *input = &inputs[0];
        for (int i = 0; i < inputs.size(); i++) {
            float gain = *(input->gain->run(current_block));
            float gprev = input->prev_gain;
            float gincr = (gain - gprev) * BL_RECIP;
            Sample_ptr inp = input->input->run(current_block);
            float *out = out_samps;
            if (gincr < 1e-6) {  // gain is constant, no interpolation
                 for (int ch = 0; ch < input->input->chans; ch++) {
                    for (int j = 0; j < BL; j++) {
                        *out++ += gain * *inp++;
                    }
                    if (out >= out_samps + BL * chans) {   // wrap to output
                        out = out_samps;
                    }
                }
           } else {  // gain is changing; do interpolation
                for (int ch = 0; ch < input->input->chans; ch++) {
                    float g = gprev;
                    for (int j = 0; j < BL; j++) {
                        g += gincr;
                        *out++ += g * *inp++;
                    }
                    if (out >= out_samps + BL * chans) {   // wrap to output
                        out = out_samps;
                    }
                }
            }
            input->prev_gain = gain;
            input++;
        }
    }
};
