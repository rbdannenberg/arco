//
//  chorddetect.cpp
//
//  Created by Mark Zhou on 3/25/24.
//

#include "arcougen.h"
#include "chorddetect.h"


const char *Chorddetect_name = "Chorddetect";

void Chorddetect::real_run()
{
    input_samps = input->run(current_block);
    // check termination of input?
    
    // convert input samples to double and process with chromagram
    double frame[BL];
    for (int i = 0; i < BL; i++) {
        frame[i] = *input_samps++;
    }
    
    chromagram.processAudioFrame(frame);
    
    if (chromagram.isReady()) { // only runs if we have enough samples
        std::vector<double> chroma = chromagram.getChromagram();
        chord_detector.detectChord(chroma);
        // send message with chord information
        o2sm_send_start();
        o2sm_add_int32(chord_detector.rootNote);
        const char* q = ChordQualityToString(chord_detector.quality);
        o2sm_add_string(q);
        o2sm_send_finish(0, cd_reply_addr, false);
    }
    
}


void Chorddetect::start(const char *reply_addr) {
    if (cd_reply_addr) {
        O2_FREE(cd_reply_addr);
    }
    cd_reply_addr = O2_MALLOCNT(strlen(reply_addr) + 1, char);
    strcpy(cd_reply_addr, reply_addr);
}

// ChordQuality is an enum, but we are sending string message to O2
// This converts between the two
const char* ChordQualityToString(int quality)
{
    switch (quality)
    {
    case ChordDetector::Minor:
        return "Minor";
    case ChordDetector::Major:
        return "Major";
    case ChordDetector::Suspended:
        return "Suspended";
    case ChordDetector::Dominant:
        return "Dominant";
    case ChordDetector::Dimished5th:
        return "Diminished 5th";
    case ChordDetector::Augmented5th:
        return "Augmented 5th";
    }
}

/* O2SM INTERFACE: /arco/chorddetect/start int32 id, string reply_addr;
 */

static void arco_chorddetect_start(O2SM_HANDLER_ARGS)
{
    
    // begin unpack message (machine-generated):
    // end unpack message

//    UGEN_FROM_ID(Chorddetect, chorddetect, id, "arco_chorddetect_start");
//    chorddetect->start(reply_addr);
}


/* O2SM INTERFACE: /arco/chorddetect/repl_input int32 id, int32 input_id;
 */

static void arco_chorddetect_repl_input(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    // end unpack message

//    UGEN_FROM_ID(Chorddetect, chorddetect, id, "arco_chorddetect_repl_input");
//    ANY_UGEN_FROM_ID(input, input_id, "arco_chorddetect_repl_input");
//    chorddetect->repl_input(input);
    //printf("chorddetect input set to %p (%s)\n", ugen, ugen->classname());
}



/* O2SM INTERFACE: /arco/chorddetect/new int32 id, int32 chans,
                                string reply_addr;
 */

static void arco_chorddetect_new(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    // end unpack message

//    new Chorddetect(id, chans, reply_addr);
}


static void chorddetect_init()
{
    // O2SM INTERFACE INITIALIZATION: (machine generated)
    // END INTERFACE INITIALIZATION
}



// Initializer chorddetect_init_obj(chorddetect_init);
