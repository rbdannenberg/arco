//
//  spectralrolloff.h
//
//

#ifndef __spectralrolloff_H__
#define __spectralrolloff_H__

#include "FFTCalculator.h"

extern const char *SpectralRolloff_name;

class SpectralRolloff : public Ugen {
public:
    char *cd_reply_addr; // Reply address for O2
    
    Ugen_ptr input;
    int input_stride;
    Sample_ptr input_samps;
    FFTCalculator fftcalc;
    float threshold;  // Percentage of spectral energy contained below
                      // the result frequency
    
    SpectralRolloff(int id, Ugen_ptr input, char *reply_addr, 
                    float threshold = 0.85) :
            Ugen(id, 0, 0), fftcalc(BL, AR), threshold(threshold) {
        cd_reply_addr = NULL;
        init_input(input);
        start(reply_addr);
    }


    ~SpectralRolloff() {
        input->unref();
        if (cd_reply_addr) {
            O2_FREE(cd_reply_addr);
        }
        
    }

    
    const char *classname() {
        return SpectralRolloff_name;
    }
    
    void print_details(int indent) {
        arco_print("SpectralRolloff running with threshold %f\n", threshold);
    }
    
    
    void print_sources(int indent, bool print_flag) {
        input->print_tree(indent, print_flag, "input");
    }


    void init_input(Ugen_ptr ugen) {
        assert(ugen->rate == 'a');
        init_param(ugen, input, &input_stride);
    }


    void repl_input(Ugen_ptr ugen) {
        input->unref();
        if (ugen->chans > 1) {
            printf("WARNING: Input has more than one channel, only the first "
                   "channel is used for spectral rolloff calculation.\n");
        }
        init_input(ugen);
    }

    void start(const char *reply_addr);

    void real_run();
};

#endif

