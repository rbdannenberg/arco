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

    Thru(int id, int nchans, Ugen_ptr inp) : Ugen(id, 'a', nchans) {
        printf("Thru@%p created, id %d, ugen_table[id] %p\n", this, id, ugen_table[id]);
        init_input(inp);
    };

    ~Thru() { input->unref(); }

    const char *classname() { return Thru_name; }

    void print_sources(int indent, bool print) {
        input->print_tree(indent, print, "input");
    }

    void set_input(Ugen_ptr inp) {
        input->unref();
        init_input(inp);
    }

    void init_input(Ugen_ptr inp) { init_param(inp, input, input_stride); }

    void real_run() {
        Sample_ptr src = input->run(current_block);  // bring input up-to-date
        for (int i = 0; i < chans; i++) {
            memcpy(out_samps, src, BLOCK_BYTES);
            src += BL + input_stride;
            out_samps += BL;
        }
    }
};
