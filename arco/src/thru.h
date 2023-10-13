/* thru.h -- unit generator that passes audio through
 *
 * Roger B. Dannenberg
 * Jan 2022
 */

extern const char *Thru_name;

class Thru : public Ugen {
public:
    Ugen_ptr input;
    int input_stride;
    Ugen_ptr alternate;  // allow input to come from another Ugen

    Thru(int id, int nchans, Ugen_ptr input) : Ugen(id, 'a', nchans) {
        printf("Thru@%p created, id %d, ugen_table[id] %p\n",
               this, id, ugen_table[id]);
        init_input(input);
        alternate = NULL;
    };

    ~Thru() { input->unref(); }

    const char *classname() { return Thru_name; }

    void print_sources(int indent, bool print_flag) {
        input->print_tree(indent, print_flag, "input");
    }

    void repl_inp(Ugen_ptr ugen) {
        input->unref();
        init_input(ugen);
    }

    void set_alternate(Ugen_ptr alt) {
        assert(alt);
        if (alternate) {
            alternate->unref();
        }
        if (alt->id == ZERO_ID) {  // clear the alternate by setting to Zero
            alternate = NULL;
            return;
        }
        if (alt->chans != chans && alt->chans != 1) {
            arco_warn("Thru set_alternate channel mismatch, not set");
            return;
        }
        alternate = alt;
        alternate->ref();
    }


    void init_input(Ugen_ptr ugen) { init_param(ugen, input, input_stride); }


    void real_run() {
        Sample_ptr src = input->run(current_block);  // bring input up-to-date
        if (!alternate) {  // redirect caller to get samples from alternate
            for (int i = 0; i < chans; i++) {
                block_copy(out_samps, src);
                src += input_stride;
                out_samps += BL;
            }
        } else {  // redirect caller to get samples from alternate
            Sample_ptr src = alternate->run(current_block);
            Sample_ptr src_end = src + BL * alternate->chans;
            int stride = (alternate->chans == 1 ? 0 : BL);
            int i;
            for (i = 0; i < chans; i++) {  // does fan-out if 1 input channel
                block_copy(out_samps, src);
                src += stride;
                out_samps += BL;
                if (src == src_end) {  // more output than input channels
                    break;
                }
            }
            if (i < chans) {  // zero-pad input to fill out remaining channels
                block_zero_n(out_samps, chans - i);
            }
        }
    }
};
