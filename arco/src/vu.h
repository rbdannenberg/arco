/* vu.h -- peak probe for vu meters
 *
 * Roger B. Dannenberg
 * Apr 2023
 */

#ifndef __Vu_H__
#define __Vu_H__

extern const char *Vu_name;

class Vu : public Ugen {
public:
    char *vu_reply_addr;
    bool running;
    Vec<Sample> peaks;
    int peak_count;
    int peak_window;
        
    Ugen_ptr inp;
    int inp_stride;
    Sample_ptr inp_samps;

    // setting output type to 0 because there is no output. chans is ignored.
    Vu(int id, int chans, char *reply_addr, float period) : Ugen(id, 0, 0) {
        vu_reply_addr = NULL;
        inp = NULL;
        running = false;
        peak_count = 0;
        peak_window = 10000;  // will be overwritten
        start(reply_addr, period);
    }

    ~Vu() {
        if (inp) {
            inp->unref();
        }
        if (vu_reply_addr) {
            O2_FREE(vu_reply_addr);
        }
        running = false;  // just in case run_set still has a reference
    }

    const char *classname() { return Vu_name; }

    void print_sources(int indent, bool print) {
        inp->print_tree(indent, print, "inp");
    }

    void repl_inp(Ugen_ptr ugen) {
        if (inp) {
            inp->unref();
        }
        assert(ugen->rate == 'a');
        init_param(ugen, inp, inp_stride);
        chans = ugen->chans;
        peaks.set_size(chans);  // initialize, set size, zero fill
        running = (vu_reply_addr != NULL && ugen->classname() != Zero_name);
    }

    void start(char *reply_addr, float period);

    void real_run();
};

#endif
