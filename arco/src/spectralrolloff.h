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
    
    SpectralRolloff(int id, char *reply_addr,
                    float threshold = 0.85) :
            Ugen(id, 0, 0), fftcalc(BL, AR), threshold(threshold) {
        printf("SpectralRolloff constructor id %d classname %s\n",
               id, classname());
        cd_reply_addr = NULL;
        input = NULL;
        start(reply_addr);
    }

    ~SpectralRolloff() {
        printf("~SpectralRolloff called. id %d input->id %d\n", id, input->id);
        if (input) {
            input->unref();
        }
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


    void repl_input(Ugen_ptr ugen) {
        if (input) {
            input->unref();
        }
        assert(ugen->rate == 'a');
        init_param(ugen, input, &input_stride);
        if (ugen->chans > 1) {
            printf("WARNING: Input has more than one channel, only the first "
                   "channel is used for spectral rolloff calculation.\n");
        }
    }

    void start(const char *reply_addr);

    void real_run();
};

#endif

