/* sum.h -- unit generator that sums inputs
 *
 * Roger B. Dannenberg
 * Nov 2023
 */

extern const char *Sum_name;

class Sum : public Ugen {
public:
    float gain;
    float prev_gain;
    bool wrap;

    Vec<Ugen_ptr> inputs;

    Sum(int id, int nchans, int wrap_) : Ugen(id, 'a', nchans) {
        gain = 1;
        prev_gain = 1;
        wrap = (wrap_ != 0); };

    ~Sum() {
        for (int i = 0; i < inputs.size(); i++) {
            inputs[i]->unref();
        }
        // since inputs is a member, ~Vec will run now and delete it
    }

    const char *classname() { return Sum_name; }


    void print_sources(int indent, bool print_flag) {
        for (int i = 0; i < inputs.size(); i++) {
            char name[8];
            snprintf(name, 8, "%d", i);
            inputs[i]->print_tree(indent, print_flag, name);
        }
    }


    // insert operation takes a signal and a gain
    void ins(Ugen_ptr input) {
        assert(input->chans > 0);
        if (input->rate != 'a') {
            arco_warn("sum_ins: input rate is not 'a', ignore insert");
            return;
        }
        int i = find(input, false);
        if (i < 0) {  // input is not already in sum; append it
            inputs.push_back(input);
            input->ref();
        }
        printf("After insert, sum inputs (%p) has\n", inputs.get_array());
        for (i = 0; i < inputs.size(); i++) {
            printf("    %p: %s\n", inputs[i], inputs[i]->classname());
        }
    }


    // find the index of Ugen input -- linear search
    int find(Ugen_ptr input, bool expected = true) {
        for (int i = 0; i < inputs.size(); i++) {
            if (inputs[i] == input) {
                return i;
            }
        }
        if (expected) {
            arco_warn("Sum::find: %p not found, nothing was changed", input);
        }
        return -1;
    }
        

    // remove operation finds the signal and removes it and its gain
    void rem(Ugen_ptr input) {
        int i = find(input);
        if (i >= 0) {
            inputs[i]->unref();
            inputs.remove(i);
        }
    }


    void swap(Ugen_ptr ugen, Ugen_ptr replacement) {
        int loc = find(ugen, false);
        if (loc == -1) {
            arco_warn("/arco/sum/swap id (%d) not in output set, ignored\n",
                      id);
            return;
        }
        ugen->unref();
        inputs[loc] = replacement;
        replacement->ref();
    }


    void real_run() {
        int starting_size = inputs.size();
        int i = 0;
        bool copy_first_input = true;
        while (i < inputs.size()) {
            Ugen_ptr input = inputs[i];
            Sample_ptr input_ptr = input->run(current_block);
            if (input->flags & TERMINATED) {
                input->unref();
                inputs.remove(i);
                continue;
            }
            i++;
            int ch = input->chans;
            if (copy_first_input) {
                block_copy_n(out_samps, input_ptr, MIN(ch, chans));
                if (ch < chans) {  // if more output channels than
                    // input channels, zero fill; but if there is only one
                    // input channel, copy it to all outputs
                    //if (ch == 1) {
                    //    for (int i = 1; i < n; i++) {
                    //        block_copy(out_samps + BL * i, input_ptr);
                    //    }
                    //} else {
                    block_zero_n(out_samps + BL * ch, chans - ch);
                    //}
                }
                copy_first_input = false;  // from now on, need to sum input
            } else {
                block_add_n(out_samps, input_ptr, MIN(ch, chans));
            }
            // whether we copied or sumed the first chans channels of input,
            // there could be extra channels to "wrap" and now we have to sum
            if (ch > chans && wrap) {
                for (int c = chans; c < ch; c += chans) {
                    block_add_n(out_samps, input_ptr + c * BL,
                                MIN(ch - c, chans));
                }
            }
        }
        if (copy_first_input) {  // did not find even first input to copy
            block_zero_n(out_samps, chans);  // zero the outputs
            // Check starting_size so that if we entered real_run() with no
            // inputs, we will not terminate. Only terminate if there was at
            // least one input that terminated and now there are none:
            if (starting_size > 0 && (flags & CAN_TERMINATE)) {
                terminate();
            }
        }
        // scale output by gain. Gain change is limited to at least 50 msec
        // for a full-scale (0 to 1) change, and special cases of constant
        // gain and unity gain are implemented:
        float gincr = (gain - prev_gain) * BL_RECIP;
        float abs_gincr = fabs(gincr);
        if (abs_gincr < 1e-6) {
            if (gain != 1) {
                for (int i = 0; i < chans * BL; i++) {
                    *out_samps++ *= gain;
                }
                prev_gain = gain;
            }
        } else {
            // we want abs_gincr * 0.050 * AR < 1, so abs_gincr < AP / 0.050
            if (abs_gincr > AP / 0.050) {
                gincr = copysignf(AP / 0.050, gincr);  // copysign(mag, sgn)
                // apply ramp to each channel:
                float g;
                for (int ch = 0; ch < chans; ch++) {
                    g = prev_gain;
                    for (int i = 0; i < BL; i++) {
                        g += gincr;
                        *out_samps++ *= g;
                    }
                }
                // due to rate limiting, the end of the ramp over BL
                // samples may not reach gain, so prev_gain is set to g
                prev_gain = g;
            }
        }
    }
};
