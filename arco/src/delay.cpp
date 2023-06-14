/* delay -- unit generator for arco
 *
 * Roger B. Dannenberg
 * Oct 2022
 */

#include "arcougen.h"
#include "delay.h"

const char *Delay_name = "Delay";

/* O2SM INTERFACE: /arco/delay/new int32 id, int32 chans, 
                                   int32 inp, int32 dur, int32 fb, float maxdur;
 */
void arco_delay_new(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t chans = argv[1]->i;
    int32_t inp = argv[2]->i;
    int32_t dur = argv[3]->i;
    int32_t fb = argv[4]->i;
    float maxdur = argv[5]->f;
    // end unpack message
    
    ANY_UGEN_FROM_ID(inp_ugen, inp, "arco_delay_new");
    ANY_UGEN_FROM_ID(dur_ugen, dur, "arco_delay_new");
    ANY_UGEN_FROM_ID(fb_ugen, fb, "arco_delay_new");

    new Delay(id, chans, inp_ugen, dur_ugen, fb_ugen, maxdur);
}


/* O2SM INTERFACE: /arco/delay/set_max int32 id, float dur;
 */
void arco_delay_set_max(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    float dur = argv[1]->f;
    // end unpack message

    UGEN_FROM_ID(Delay, delay, id, "arco_delay_set_max");
    delay->set_max(dur);
}


/* O2SM INTERFACE: /arco/delay/repl_dur int32 id, int32 dur_id;
 */
static void arco_delay_repl_dur(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t dur_id = argv[1]->i;
    // end unpack message

    UGEN_FROM_ID(Delay, delay, id, "arco_delay_repl_dur");
    ANY_UGEN_FROM_ID(dur, dur_id, "arco_delay_repl_dur");
    delay->repl_dur(dur);
}


/* O2SM INTERFACE: /arco/delay/set_dur int32 id, int32 chan, float dur;
 */
static void arco_delay_set_dur(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t chan = argv[1]->i;
    float dur = argv[2]->f;
    // end unpack message

    UGEN_FROM_ID(Delay, delay, id, "arco_delay_repl_dur");
    delay->set_dur(chan, dur);
}


/* O2SM INTERFACE: /arco/delay/repl_fb int32 id, int32 fb_id;
 */
static void arco_delay_repl_fb(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t fb_id = argv[1]->i;
    // end unpack message

    UGEN_FROM_ID(Delay, delay, id, "arco_delay_repl_fb");
    ANY_UGEN_FROM_ID(fb, fb_id, "arco_delay_repl_fb");
    delay->repl_fb(fb);
}


/* O2SM INTERFACE: /arco/delay/set_fb int32 id, int32 chan, float fb;
 */
static void arco_delay_set_fb(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t chan = argv[1]->i;
    float fb = argv[2]->f;
    // end unpack message

    UGEN_FROM_ID(Delay, delay, id, "arco_delay_repl_fb");
    delay->set_fb(chan, fb);
}


static void delay_init()
{
    // O2SM INTERFACE INITIALIZATION: (machine generated)
    o2sm_method_new("/arco/delay/new", "iiiiif", arco_delay_new, NULL, true, true);
    o2sm_method_new("/arco/delay/set_max", "if", arco_delay_set_max, NULL, true, true);
    o2sm_method_new("/arco/delay/repl_dur", "ii", arco_delay_repl_dur, NULL, true, true);
    o2sm_method_new("/arco/delay/set_dur", "iif", arco_delay_set_dur, NULL, true, true);
    o2sm_method_new("/arco/delay/repl_fb", "ii", arco_delay_repl_fb, NULL, true, true);
    o2sm_method_new("/arco/delay/set_fb", "iif", arco_delay_set_fb, NULL, true, true);
    // END INTERFACE INITIALIZATION
}

Initializer delay_init_obj(delay_init);
