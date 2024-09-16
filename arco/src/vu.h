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
        
    Ugen_ptr input;
    int input_stride;
    Sample_ptr input_samps;

    // setting output type to 0 because there is no output. chans is ignored.
    Vu(int id, int chans, char *reply_addr, float period) : Ugen(id, 0, 0) {
        printf("Vu constructor id %d classname %s\n", id, classname());
        vu_reply_addr = NULL;
        input = NULL;
        running = false;
        peak_count = 0;
        peak_window = 10000;  // will be overwritten
        start(reply_addr, period);
    }

    ~Vu() {
        printf("~Vu called. id %d input->id %d\n", id, input->id);
        if (input) {
            input->unref();
        }
        if (vu_reply_addr) {
            O2_FREE(vu_reply_addr);
        }
    }

    
    const char *classname() {
        return Vu_name;
    }

    
    void print_details(int indent) {
        arco_print("running %s window %d",
                   (running ? "true" : "false"), peak_window);
    }
    
    
    void print_sources(int indent, bool print_flag) {
        input->print_tree(indent, print_flag, "input");
    }

    
    void repl_input(Ugen_ptr ugen) {
        if (input) {
            input->unref();
        }
        assert(ugen->rate == 'a');
        init_param(ugen, input, &input_stride);
        chans = ugen->chans;
        peaks.set_size(chans);  // initialize, set size, zero fill
        running = (vu_reply_addr != NULL && ugen->classname() != Zero_name);
    }

    void start(char *reply_addr, float period);

    void real_run();
};

#endif
