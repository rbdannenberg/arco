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

    Thru(int id, int nchans, Ugen_ptr inp) : Ugen(id, 'a', nchans) {
        printf("Thru@%p created, id %d, ugen_table[id] %p\n", this, id, ugen_table[id]);
        init_input(inp);
        alternate = NULL;
    };

    ~Thru() { input->unref(); }

    const char *classname() { return Thru_name; }

    void print_sources(int indent, bool print) {
        input->print_tree(indent, print, "input");
    }

    void repl_inp(Ugen_ptr inp) {
        input->unref();
        init_input(inp);
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


    void init_input(Ugen_ptr inp) { init_param(inp, input, input_stride); }


    Sample_ptr run(int64_t block_count) {
        // first, act normal and update output if needed
        Ugen::run(block_count);
        if (alternate) {  // redirect caller to get samples from alternate
            return alternate->run(block_count);
        }
        return &output[0];
    }


    void real_run() {
        Sample_ptr src = input->run(current_block);  // bring input up-to-date
        for (int i = 0; i < chans; i++) {
            block_copy(out_samps, src);
            src += BL + input_stride;
            out_samps += BL;
        }
    }
};
