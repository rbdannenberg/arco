/* addb.h -- unit generator that addbs inputs
 *
 * Roger B. Dannenberg
 * Nov 2023
 */

extern const char *Addb_name;

class Addb : public Ugen {
public:
    Vec<Ugen_ptr> inputs;
    
    Addb(int id, int nchans) : Ugen(id, 'a', nchans) {
    };

    ~Addb() {
        for (int i = 0; i < inputs.size(); i++) {
            inputs[i]->unref();
        }
        // since inputs is a member, ~Vec will run now and delete it
    }

    const char *classname() { return Addb_name; }


    void print_sources(int indent, bool print_flag) {
        for (int i = 0; i < inputs.size(); i++) {
            char name[8];
            snprintf(name, 8, "%d", i);
            inputs[i]->print_tree(indent, print_flag, name);
        }
    }


    // insert operation takes a signal and a gain
    void ins(Ugen_ptr input) {
        if (input->rate == 'a') {
            arco_warn("addb_ins: input rate must not be 'a', ignore insert");
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
            arco_warn("Addb::find: %p not found, nothing was changed", input);
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
            arco_warn("/arco/addb/swap id (%d) not in output set, ignored\n",
                      id);
            return;
        }
        ugen->unref();
        inputs[i] = replacement;
        replacement->ref();
    }


    void real_run() {
        int n = inputs.size();
        if (n == 0) { // zero the outputs
            memset(out_samps, 0, chans * sizeof(Sample));
        } else {
            Ugen_ptr input = inputs[0];
            Sample_ptr input_ptr = input->run(current_block);
            // if more input channels than output channels, drop some input:
            int ch = MIN(input->chans, chans);
            memcpy(out_samps, input_ptr, ch * sizeof(Sample));
            // if more output channels than input channels, zero fill; except
            // if there is only one input channel, copy it to all outputs
            if (ch < chans) {
                if (ch == 1) {
                    for (int i = 1; i < n; i++) {
                        out_samps[i] = *input_ptr;
                    }
                } else {
                    block_zero_n(out_samps + ch,
                                 (chans - ch) * sizeof(Sample));
                }
            }
            // now add remaining channels
            for (int i = 1; i < n; i++) {
                input = inputs[i];
                input_ptr = input->run(current_block);
                int samps = MIN(input->chans, chans);
                for (int j = 0; j < samps; j++) {
                    *out_samps++ += *input_ptr++;
                }
                if (samps == 1 && chans > 1) {  // addb single channel to all
                    input_ptr -= 1;  // back up top beginning of input
                    for (int ch = 1; ch < chans; ch++) {
                        for (int j = 0; j < BL; j++) {
                            *out_samps++ = *input_ptr;
                        }
                    }
                }
            }
        }
    }
};
