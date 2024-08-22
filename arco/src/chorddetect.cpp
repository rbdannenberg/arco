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
    
    // Note: processAudioFrame assumes input_samps has length BL, so
    // it only processes the first channel of input and the rest are
    // ignored.
    chromagram.processAudioFrame(input_samps);
    if (chromagram.isReady()) { // only runs if we have enough samples
        
        float* chroma = chromagram.getChromagram();
        ChordDetector chord_detector;
        chord_detector.detectChord(chroma);
        // send message with chord information
        o2sm_send_start();
        o2sm_add_string(RootNoteToString(chord_detector.rootNote));
        o2sm_add_string(ChordQualityToString(chord_detector.quality));
        o2sm_add_int32(chord_detector.intervals);
        o2sm_add_float(chord_detector.confidence);
        o2sm_add_bool(chord_detector.shouldDisplay);
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

// Converts chord quality enum to string message
const char* Chorddetect::ChordQualityToString(int quality)
{
    const char* qualities[] = {"Min", "Maj","Sus", "Dom","Dim 5th", "Aug 5th", "Half-Dim"};
    return qualities[quality];
}

// Convert root note integer to string
const char* Chorddetect::RootNoteToString(int root)
{
    const char* notes[] = {"C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B"};
    return notes[root];
}

/* O2SM INTERFACE: /arco/chorddetect/start int32 id, string reply_addr;
 */
static void arco_chorddetect_start(O2SM_HANDLER_ARGS)
{

    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    char *reply_addr = argv[1]->s;
    // end unpack message

    UGEN_FROM_ID(Chorddetect, chorddetect, id, "arco_chorddetect_start");
    chorddetect->start(reply_addr);
}


/* O2SM INTERFACE: /arco/chorddetect/repl_input int32 id, int32 input_id;
 */
static void arco_chorddetect_repl_input(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t input_id = argv[1]->i;
    // end unpack message

    UGEN_FROM_ID(Chorddetect, chorddetect, id, "arco_chorddetect_repl_input");
    ANY_UGEN_FROM_ID(input, input_id, "arco_chorddetect_repl_input");
    chorddetect->repl_input(input);
    // printf("chorddetect input set to %p (%s)\n", ugen, ugen->classname());
}



/* O2SM INTERFACE: /arco/chorddetect/new int32 id, int32 chans, string reply_addr;
 */
static void arco_chorddetect_new(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t chans = argv[1]->i;
    char *reply_addr = argv[2]->s;
    // end unpack message

    new Chorddetect(id, chans, reply_addr);
}


static void chorddetect_init()
{
    // O2SM INTERFACE INITIALIZATION: (machine generated)
    o2sm_method_new("/arco/chorddetect/start", "is", arco_chorddetect_start,
                    NULL, true, true);
    o2sm_method_new("/arco/chorddetect/repl_input", "ii",
                    arco_chorddetect_repl_input, NULL, true, true);
    o2sm_method_new("/arco/chorddetect/new", "iis", arco_chorddetect_new,
                    NULL, true, true);
    // END INTERFACE INITIALIZATION
}



Initializer chorddetect_init_obj(chorddetect_init);

