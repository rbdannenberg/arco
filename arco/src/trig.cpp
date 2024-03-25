// trig.cpp -- trig unit generator for sound event detection
//
// Roger B. Dannenberg
// Aug 2023

/* trig is an audio unit generator that sends messages when
   RMS exceeds a threshold or when RMS indicates sound on
   and sound off.
*/

#include <cmath>
#include "arcougen.h"
#include "trig.h"

const char *Trig_name = "Trig";


/* O2SM INTERFACE: /arco/trig/new int32 id, int32 input_id, string reply_addr,
      int32 window_size, float threshold, float pause;
 */
void arco_trig_new(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t input_id = argv[1]->i;
    char *reply_addr = argv[2]->s;
    int32_t window_size = argv[3]->i;
    float threshold = argv[4]->f;
    float pause = argv[5]->f;
    // end unpack message

    ANY_UGEN_FROM_ID(input, input_id, "arco_trig_new");
    new Trig(id, input, reply_addr, window_size, threshold, pause);
}


/* O2SM INTERFACE: /arco/trig/onoff int32 id, string repl_addr, 
                                    float threshold, float runlen;
 */
void arco_trig_onoff(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    char *repl_addr = argv[1]->s;
    float threshold = argv[2]->f;
    float runlen = argv[3]->f;
    // end unpack message

    UGEN_FROM_ID(Trig, trig, id, "arco_trig_onoff");
    trig->onoff(repl_addr, threshold, runlen);
}


/* O2SM INTERFACE: /arco/trig/repl_input int32 id, int32 input_id;
 */
void arco_trig_repl_input(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t input_id = argv[1]->i;
    // end unpack message

    UGEN_FROM_ID(Trig, trig, id, "arco_trig_repl_input");
    ANY_UGEN_FROM_ID(input, input_id, "arco_trig_repl_input");
    trig->repl_input(input);
}


/* O2SM INTERFACE: /arco/trig/window int32 id, int32 size;
 */
void arco_trig_window(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t size = argv[1]->i;
    // end unpack message

    UGEN_FROM_ID(Trig, trig, id, "arco_trig_window");
    trig->set_window(size);
}


/* O2SM INTERFACE: /arco/trig/thresh int32 id, float thresh;
 */
void arco_trig_thresh(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    float thresh = argv[1]->f;
    // end unpack message

    UGEN_FROM_ID(Trig, trig, id, "arco_trig_thresh");
    trig->set_threshold(thresh);
}
    

/* O2SM INTERFACE: /arco/trig/pause int32 id, float pause;
 */
void arco_trig_pause(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    float pause = argv[1]->f;
    // end unpack message

    UGEN_FROM_ID(Trig, trig, id, "arco_trig_pause");
    trig->set_pause(pause);
}
    

static void trig_init()
{
    // O2SM INTERFACE INITIALIZATION: (machine generated)
    o2sm_method_new("/arco/trig/new", "iisiff", arco_trig_new, NULL, true,
                    true);
    o2sm_method_new("/arco/trig/onoff", "isff", arco_trig_onoff, NULL, true,
                    true);
    o2sm_method_new("/arco/trig/repl_input", "ii", arco_trig_repl_input,
                    NULL, true, true);
    o2sm_method_new("/arco/trig/window", "ii", arco_trig_window, NULL, true,
                    true);
    o2sm_method_new("/arco/trig/thresh", "if", arco_trig_thresh, NULL, true,
                    true);
    o2sm_method_new("/arco/trig/pause", "if", arco_trig_pause, NULL, true,
                    true);
    // END INTERFACE INITIALIZATION
}


Initializer trig_init_obj(trig_init);
