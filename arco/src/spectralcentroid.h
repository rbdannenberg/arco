//
//  spectralcentroid.h
//
//

#ifndef __spectralcentroid_H__
#define __spectralcentroid_H__

#include "FFTCalculator.h"

extern const char *SpectralCentroid_name;

class SpectralCentroid : public Ugen {
public:
    char *cd_reply_addr; // Reply address for O2
    
    Ugen_ptr input;
    int input_stride;
    Sample_ptr input_samps;
    FFTCalculator fftcalc;
    
    SpectralCentroid(int id, int chans, char *reply_addr) : Ugen(id, 0, 0), fftcalc(BL, AR)
                {
        printf("SpectralCentroid constructor id %d classname %s\n", id, classname());
        cd_reply_addr = NULL;
        input = NULL;
        start(reply_addr);
    }

    ~SpectralCentroid() {
        printf("~SpectralCentroid called. id %d input->id %d\n", id, input->id);
        if (input) {
            input->unref();
        }
        if (cd_reply_addr) {
            O2_FREE(cd_reply_addr);
        }
        
    }

    
    const char *classname() {
        return SpectralCentroid_name;
    }
    
    void print_details(int indent) {
        arco_print("SpectralCentroid running.\n");
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
        if (chans > 1) {
            printf("WARNING: Input has more than one channel, only the first channel is used for spectral centroid calculation.\n");
        }
    }

    void start(const char *reply_addr);

    void real_run();
};

#endif
