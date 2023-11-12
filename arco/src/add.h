/* add.h -- unit generator that adds inputs
 *
 * Roger B. Dannenberg
 * Nov 2023
 */

extern const char *Add_name;

class Add : public Ugen {
public:
    Vec<Ugen_ptr> inputs;

    Add(int id, int nchans) : Ugen(id, 'a', nchans) { ; };

    ~Add() {
        for (int i = 0; i < inputs.size(); i++) {
            inputs[i]->unref();
        }
        // since inputs is a member, ~Vec will run now and delete it
    }

    const char *classname() { return Add_name; }


    void print_sources(int indent, bool print_flag) {
        for (int i = 0; i < inputs.size(); i++) {
            char name[8];
            snprintf(name, 8, "%d", i);
            inputs[i]->print_tree(indent, print_flag, name);
        }
    }


    // insert operation takes a signal and a gain
    void ins(Ugen_ptr input) {
        if (input->rate != 'a') {
            arco_warn("add_ins: input rate is not 'a', ignore insert");
            return;
        }
        int i = find(input, false);
        if (i < 0) {  // input is not already in sum; append it
            inputs.push_back(input);
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
            arco_warn("Add::find: %p not found, nothing was changed", input);
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


    void real_run() {
        int n = inputs.size();
        if (n == 0) { // zero the outputs
            block_zero_n(out_samps, chans);
        } else {
            Ugen_ptr input = inputs[0];
            Sample_ptr input_ptr = input->run(current_block);
            // if more input channels than output channels, drop some input:
            int ch = MIN(input->chans, chans);
            block_copy_n(out_samps, input_ptr, ch);
            // if more output channels than input channels, zero fill; except
            // if there is only one input channel, copy it to all outputs
            if (ch < chans) {
                if (ch == 1) {
                    for (int i = 1; i < n; i++) {
                        block_copy(out_samps + BL * i, input_ptr);
                    }
                } else {
                    block_zero_n(out_samps + BL * ch, chans - ch);
                }
            }
            // now add remaining inputs
            for (int i = 1; i < n; i++) {
                input = inputs[i];
                input_ptr = input->run(current_block);
                int samps = MIN(input->chans, chans) * BL;
                for (int j = 0; j < samps; j++) {
                    *out_samps++ += *input_ptr++;
                }
                if (samps == BL && chans > 1) {  // add single channel to all
                    for (int ch = 1; ch < chans; ch++) {
                        input_ptr -= BL;  // back up top beginning of input
                        for (int j = 0; j < BL; j++) {
                            *out_samps++ = *input_ptr++;
                        }
                    }
                }
            }
        }
    }
};
