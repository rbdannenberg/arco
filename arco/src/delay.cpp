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
    // end unpack message

    new Delay(id, chans, inp, dur, feedback, maxdur);
}


/* O2SM INTERFACE: /arco/delay/set_max int32 id, float dur;
 */
void arco_delay_set_max(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    // end unpack message
    UGEN_FROM_ID(Delay, delay, id, "arco_delay_set_max");
    delay->set_max(dur);
}


/* /arco/delay/repl_dur int32 id int32 dur_id;
 */
static void arco_delay_repl_dur(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    // end unpack message
    UGEN_FROM_ID(Delay, delay, id, "arco_delay_repl_dur");
    ANY_UGEN_FROM_ID(dur, dur_id, "arco_delay_repl_dur");
    delay->repl_dur(dur);
}


/* /arco/delay/set_dur int32 id int32 chan float dur;
 */
static void arco_delay_set_dur(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    // end unpack message
    UGEN_FROM_ID(Delay, delay, id, "arco_delay_repl_dur");
    delay->set_dur(chan, dur);
}


/* /arco/delay/repl_fb int32 id int32 fb_id;
 */
static void arco_delay_repl_fb(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    // end unpack message
    UGEN_FROM_ID(Delay, delay, id, "arco_delay_repl_fb");
    ANY_UGEN_FROM_ID(fb, fb_id, "arco_delay_repl_fb");
    delay->repl_fb(fb);
}


/* /arco/delay/set_fb int32 id int32 chan float fb;
 */
static void arco_delay_set_fb(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    // end unpack message
    UGEN_FROM_ID(Delay, delay, id, "arco_delay_repl_fb");
    delay->set_fb(chan, fb);
}


static void delay_init()
{
    // O2SM INTERFACE INITIALIZATION: (machine generated)
    o2sm_method_new("/arco/delay/new", "i", arco_pwl_new, NULL, true, true);
    o2sm_method_new("/arco/delay/start", "i", arco_pwl_start, NULL, true, true);
    o2sm_method_new("/arco/delay/decay", "if", arco_pwl_decay, NULL, true, true);
    // END INTERFACE INITIALIZATION
}

Initializer delay_init_obj(delay_init);
