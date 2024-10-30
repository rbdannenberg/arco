/* sumb.h -- unit generator that sumbs inputs
 *
 * Roger B. Dannenberg
 * Nov 2023
 */

extern const char *Sumb_name;

class Sumb : public Ugen {
public:
    Vec<Ugen_ptr> inputs;
    
    Sumb(int id, int nchans) : Ugen(id, 'a', nchans) {
    };

    ~Sumb() {
        for (int i = 0; i < inputs.size(); i++) {
            inputs[i]->unref();
        }
        // since inputs is a member, ~Vec will run now and delete it
    }

    const char *classname() { return Sumb_name; }


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
            arco_warn("sumb_ins: input rate must not be 'a', ignore insert");
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
            arco_warn("Sumb::find: %p not found, nothing was changed", input);
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
            arco_warn("/arco/sumb/swap id (%d) not in output set, ignored\n",
                      id);
            return;
        }
        ugen->unref();
        inputs[loc] = replacement;
        replacement->ref();
    }


    void real_run() {
        memset(out_samps, 0, chans * sizeof(Sample));
        int i = 0;
        while (i < inputs.size()) {
            Ugen_ptr input = inputs[i];
            Sample_ptr input_ptr = input->run(current_block);
            if (input->flags & TERMINATED) {
                send_action_id(ACTION_REM, input->id);
                input->unref();
                inputs.remove(i);
                continue;
            }
            i++;
            int samps = MIN(input->chans, chans);
            for (int j = 0; j < samps; j++) {
                *out_samps++ += *input_ptr++;
            }
            if (samps == 1 && chans > 1) {  // sumb single channel to all
                input_ptr -= 1;  // back up top beginning of input
                for (int ch = 1; ch < chans; ch++) {
                    *out_samps++ = *input_ptr;
                }
            }
        }
    }
};
