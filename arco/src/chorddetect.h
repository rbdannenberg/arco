//
//  chorddetect.h
//
//  Created by Mark Zhou on 3/25/24.
//

#ifndef __chorddetect_H__
#define __chorddetect_H__

#include "ChordDetector.h"
#include "Chromagram.h"
#include <string>

extern const char *Chorddetect_name;

class Chorddetect : public Ugen {
public:
    char *cd_reply_addr; // Reply address for O2
    
    Chromagram chromagram;
    ChordDetector chord_detector;
    
    Ugen_ptr input;
    int input_stride;
    Sample_ptr input_samps;

    // setting output type to 0 because there is no output. chans is ignored.
    Chorddetect(int id, int chans, char *reply_addr, int frame_size,
                int sample_rate) : Ugen(id, 0, 0), chromagram(frame_size, sample_rate)
                {
        printf("Chorddetect constructor id %d classname %s\n", id, classname());
        cd_reply_addr = NULL;
        input = NULL;
        chromagram.setInputAudioFrameSize(frame_size);
        chromagram.setSamplingFrequency(sample_rate);
        start(reply_addr);
    }

    ~Chorddetect() {
        printf("~Chorddetect called. id %d input->id %d\n", id, input->id);
        if (input) {
            input->unref();
        }
        if (cd_reply_addr) {
            O2_FREE(cd_reply_addr);
        }
    }

    
    const char *classname() {
        return Chorddetect_name;
    }

    const char* ChordQualityToString(int quality);
    
    void print_details(int indent) {
        // Not finished 
        arco_print("Running");
    }
    
    
    void print_sources(int indent, bool print_flag) {
        input->print_tree(indent, print_flag, "input");
    }

    
    void repl_input(Ugen_ptr ugen) {
        if (input) {
            input->unref();
        }
        assert(ugen->rate == 'a');
        init_param(ugen, input, input_stride);
        chans = ugen->chans;
    }

    void start(const char *reply_addr);

    void real_run();
};

#endif
