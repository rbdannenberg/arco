/* mix.h -- unit generator that passes audio through
 *
 * Roger B. Dannenberg
 * Feb 2022
 */

extern const char *Mix_name;

class Mix : public Ugen {
public:
    struct Input {
        const char *name;
        Ugen_ptr input;
        int input_stride;
        Ugen_ptr gain;
        int gain_stride;
        Vec<float> prev_gain;
    };
    Vec<Input> inputs;
    

    Mix(int id, int nchans) : Ugen(id, 'a', nchans) {
    };

    ~Mix();

    const char *classname() { return Mix_name; }

    void print_sources(int indent, bool print);

    // insert operation takes a signal and a gain
    void ins(const char *name, Ugen_ptr inp, Ugen_ptr gain) {
        if (inp->rate != 'a') {
            arco_warn("mix_ins: input rate is not 'a', ignore insert");
            return;
        }
        if (gain->rate == 'a') {
            arco_warn("mix_ins: gain rate must be 'b' or 'c', ignore insert");
            return;
        }
        if (gain->chans != 1 && inp->chans != 1 && gain->chans != inp->chans) {
            arco_warn("mix_ins: inp has %d chans, gain has %d chans",
                      inp->chans, gain->chans);
            return;
        }
        int i = find(name, false);
        Input *input;
        if (i >= 0) {  // name already exists; replace input
            input = &inputs[i];
            input->input->unref();
            input->input = NULL;
            input->gain->unref();
            input->gain = NULL;
        } else {  // allocate space for new input and initialize
            input = inputs.append_space(1);
            input->prev_gain.init(0);
            input->name = o2_heapify(name);
        }

        input->name = o2_heapify(name);
        int ignore_stride;  // inputs are always audio and stride is always 1
        init_param(inp, input->input, input->input_stride);
        init_param(gain, input->gain, input->gain_stride);
        // initialize Vec of per-channel prev_gain value(s) and zero them:
        input->prev_gain.set_size(MAX(input->input->chans, input->gain->chans),
                                  true);
        // arco_print("after Mix::ins, inp->refcount %d\n", inp->refcount);
    }


    // find the index of Ugen inp -- linear search
    int find(const char *name, bool expected = true) {
        for (int i = 0; i < inputs.size(); i++) {
            Input *input = &inputs[i];
            if (strcmp(input->name, name) == 0) {
                return i;
            }
        }
        if (expected) {
            arco_warn("Mix::find: %s not found, nothing was changed", name);
        }
        return -1;
    }
        

    // remove operation finds the signal and removes it and its gain
    void rem(char *name) {
        int i = find(name);
        if (i >= 0) {
            Input *input = &inputs[i];
            O2_FREE((void *) input->name);
            input->name = NULL;
            input->input->unref();
            input->input = NULL;
            input->gain->unref();
            input->gain = NULL;
            inputs.remove(i);
        }
    }

    void set_gain(const char *name, int chan, float gain) {
        int i = find(name);
        if (i >= 0) {
            Input *input = &inputs[i];
            Ugen *gain_ugen = input->gain;
            if (gain_ugen->rate == 'c') {
                gain_ugen->const_set(chan, gain, "Mix::set_gain");
            } else if (chan == 0) {
                gain_ugen->unref();
                init_param(new Const(-1, 1, gain), input->gain, input->gain_stride);
                input->prev_gain.set_size(input->input->chans,
                        // zero prev_gain if number of channels changes:
                        input->input->chans != input->prev_gain.size());
            } else {
                printf("WARNING: In Mix::set_gain, current gain is not Const"
                       " and chan to set is not zero, mix %d, name %s,"
                       " chan %d\n", id, name, chan);
            }
        }
    }

    void repl_gain(char *name, Ugen_ptr gain) {
        int i = find(name);
        if (i >= 0) {
            Input *input = &inputs[i];
            if (gain->chans != 1 && input->input->chans != 1 &&
                gain->chans != input->input->chans) {
                    arco_warn("mix_repl_gain: %s input has %d chans, new gain "
                              "has %d chans", name, input->input->chans,
                              gain->chans);
                    return;
            }
            input->gain->unref();
            init_param(gain, input->gain, input->gain_stride);
            int new_chans = MAX(input->input->chans, gain->chans);
            input->prev_gain.set_size(new_chans,
                    // reinit. gains to zero if mismatch in gain channel count:
                    new_chans != input->prev_gain.size());
        }
    }


    void real_run() {
        // zero the outputs
        block_zero_n(out_samps, chans);
        // add inputs with wrap-around
        Input *input = &inputs[0];
        for (int i = 0; i < inputs.size(); i++) {
            float *gain_ptr = input->gain->run(current_block);
            float *gprev_ptr = &(input->prev_gain[0]);
            Sample_ptr inp = input->input->run(current_block);
            float *out = out_samps;
            assert(input->prev_gain.size() ==
                   MAX(input->input->chans, input->gain->chans));
            for (int ch = 0; ch < input->prev_gain.size(); ch++) {
                float gain = *gain_ptr;
                float gincr = (gain - *gprev_ptr) * BL_RECIP;
            
                if (gincr < 1e-6) {  // gain is constant, no interpolation
                    for (int j = 0; j < BL; j++) {
                        *out++ += gain * *inp++;
                    }
                } else {  // gain is changing; do interpolation
                    float g = *gprev_ptr;
                    for (int j = 0; j < BL; j++) {
                        g += gincr;
                        *out++ += g * *inp++;
                    }
                    *gprev_ptr = g;
                }
                // stride is relative to starting point for this channel,
                // so subtract BL to get back to starting point:
                inp -= BL;
                inp += input->input_stride;
                if (out >= out_samps + BL * chans) {   // wrap to output
                    out = out_samps;
                }
                // advance to the next channel of this input:
                gprev_ptr++;  // we have a prev_gain for each generated channel
                        // even if gain is single channel (gain_stride == 0)
                gain_ptr += input->gain_stride;
            }
            input++; // sum the next input to mixer outputs on next iteration
        }
    }
};
