/* alpass -- unit generator for arco
 *
 * Roger B. Dannenberg
 * Oct 2022
 */

#include "arcougen.h"
#include "alpass.h"

const char *Alpass_name = "Alpass";

/* O2SM INTERFACE: /arco/alpass/new int32 id, int32 chans, 
            int32 inp, int32 dur, int32 fb, float maxdur;
 */
void arco_alpass_new(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t chans = argv[1]->i;
    int32_t inp = argv[2]->i;
    int32_t dur = argv[3]->i;
    int32_t fb = argv[4]->i;
    float maxdur = argv[5]->f;
    // end unpack message

    ANY_UGEN_FROM_ID(inp_ugen, inp, "arco_alpass_new");
    ANY_UGEN_FROM_ID(dur_ugen, dur, "arco_alpass_new");
    ANY_UGEN_FROM_ID(fb_ugen, fb, "arco_alpass_new");

    new Alpass(id, chans, inp_ugen, dur_ugen, fb_ugen, maxdur);
}


/* O2SM INTERFACE: /arco/alpass/repl_inp int32 id, int32 inp_id;
 */
static void arco_alpass_repl_inp(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t inp_id = argv[1]->i;
    // end unpack message

    UGEN_FROM_ID(Alpass, alpass, id, "arco_alpass_repl_inp");
    ANY_UGEN_FROM_ID(inp, inp_id, "arco_alpass_repl_inp");
    alpass->repl_inp(inp);
}


/* O2SM INTERFACE: /arco/alpass/max int32 id, float dur;
 */
void arco_alpass_set_max(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    float dur = argv[1]->f;
    // end unpack message

    UGEN_FROM_ID(Alpass, alpass, id, "arco_alpass_set_max");
    alpass->set_max(dur);
}


/* O2SM INTERFACE: /arco/alpass/repl_dur int32 id, int32 dur_id;
 */
static void arco_alpass_repl_dur(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t dur_id = argv[1]->i;
    // end unpack message

    UGEN_FROM_ID(Alpass, alpass, id, "arco_alpass_repl_dur");
    ANY_UGEN_FROM_ID(dur, dur_id, "arco_alpass_repl_dur");
    alpass->repl_dur(dur);
}


/* O2SM INTERFACE: /arco/alpass/set_dur int32 id, int32 chan, float dur;
 */
static void arco_alpass_set_dur(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t chan = argv[1]->i;
    float dur = argv[2]->f;
    // end unpack message

    UGEN_FROM_ID(Alpass, alpass, id, "arco_alpass_repl_dur");
    alpass->set_dur(chan, dur);
}


/* O2SM INTERFACE: /arco/alpass/repl_fb int32 id, int32 fb_id;
 */
static void arco_alpass_repl_fb(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t fb_id = argv[1]->i;
    // end unpack message

    UGEN_FROM_ID(Alpass, alpass, id, "arco_alpass_repl_fb");
    ANY_UGEN_FROM_ID(fb, fb_id, "arco_alpass_repl_fb");
    alpass->repl_fb(fb);
}


/* O2SM INTERFACE: /arco/alpass/set_fb int32 id, int32 chan, float fb;
 */
static void arco_alpass_set_fb(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t chan = argv[1]->i;
    float fb = argv[2]->f;
    // end unpack message

    UGEN_FROM_ID(Alpass, alpass, id, "arco_alpass_repl_fb");
    alpass->set_fb(chan, fb);
}


static void alpass_init()
{
    // O2SM INTERFACE INITIALIZATION: (machine generated)
    o2sm_method_new("/arco/alpass/new", "iiiiif", arco_alpass_new, NULL, true,
                     true);
    o2sm_method_new("/arco/alpass/repl_inp", "ii", arco_alpass_repl_inp, NULL,
                     true, true);
    o2sm_method_new("/arco/alpass/max", "if", arco_alpass_set_max, NULL, true,
                     true);
    o2sm_method_new("/arco/alpass/repl_dur", "ii", arco_alpass_repl_dur, NULL,
                     true, true);
    o2sm_method_new("/arco/alpass/set_dur", "iif", arco_alpass_set_dur, NULL,
                     true, true);
    o2sm_method_new("/arco/alpass/repl_fb", "ii", arco_alpass_repl_fb, NULL,
                     true, true);
    o2sm_method_new("/arco/alpass/set_fb", "iif", arco_alpass_set_fb, NULL,
                     true, true);
    // END INTERFACE INITIALIZATION
}

Initializer alpass_init_obj(alpass_init);
