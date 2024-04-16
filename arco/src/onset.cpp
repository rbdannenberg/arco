// onset.cpp -- onset unit generator for sound event detection
//
// Roger B. Dannenberg
// Aug 2023

/* onset is an audio unit generator that sends messages when
   RMS exceeds a threshold or when RMS indicates sound on
   and sound off.
*/

#include <cmath>
#include "arcougen.h"
#include "onset.h"

const char *Onset_name = "Onset";


/* O2SM INTERFACE: /arco/onset/new int32 id, int32 input_id, string reply_addr,
      int32 window_size, float threshold, float pause;
 */
void arco_onset_new(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t input_id = argv[1]->i;
    char *reply_addr = argv[2]->s;
    // end unpack message

    ANY_UGEN_FROM_ID(input, input_id, "arco_onset_new");
    new Onset(id, input, reply_addr);
}

static void onset_init()
{
    // O2SM INTERFACE INITIALIZATION: (machine generated)
    o2sm_method_new("/arco/onset/new", "iis", arco_onset_new, NULL, true,
                    true);
    // END INTERFACE INITIALIZATION
}


Initializer onset_init_obj(onset_init);
