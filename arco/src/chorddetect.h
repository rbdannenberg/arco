//
//  chorddetect.h
//
//  Created by Mark Zhou on 3/25/24.
//

#ifndef __chorddetect_H__
#define __chorddetect_H__

#include "ChordDetector.h"
#include "Chromagram.h"

extern const char *Chorddetect_name;

class Chorddetect : public Ugen {
public:
    char *cd_reply_addr; // Reply address for O2
    
    Chromagram chromagram;
    ChordDetector chord_detector;
    
    Ugen_ptr input;
    int input_stride;
    Sample_ptr input_samps;
    float threshold;
    
    Chorddetect(int id, Ugen_ptr input, char *reply_addr, 
                double display_threshold = 0.0005) :
            Ugen(id, 0, 0), chromagram(BL, AR) {
        cd_reply_addr = NULL;
        init_input(input);
        threshold = display_threshold;
        start(reply_addr);
    }

    ~Chorddetect() {
        input->unref();
        if (cd_reply_addr) {
            O2_FREE(cd_reply_addr);
        }
        
    }

    
    const char *classname() {
        return Chorddetect_name;
    }

    const char* ChordQualityToString(int quality);
    
    const char* RootNoteToString(int root);
    
    void print_details(int indent) {
        // Not finished 
        arco_print("Running");
    }
    
    
    void print_sources(int indent, bool print_flag) {
        input->print_tree(indent, print_flag, "input");
    }

    
    void repl_input(Ugen_ptr ugen) {
        input->unref();
        if (ugen->chans > 1) {
            printf("WARNING: Chorddetect input has more than one channel, only "
                   "the first channel is used for chord detection.\n");
        }
        init_input(ugen);
    }


    void init_input(Ugen_ptr ugen) {
        assert(ugen->rate == 'a');
        init_param(ugen, input, &input_stride);
    }

    void start(const char *reply_addr);

    void real_run();
};

#endif
