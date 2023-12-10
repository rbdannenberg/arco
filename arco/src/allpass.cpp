/* allpass -- unit generator for arco
 *
 * Roger B. Dannenberg
 * Oct 2022
 */

#include "arcougen.h"
#include "allpass.h"

const char *Allpass_name = "Allpass";

/* O2SM INTERFACE: /arco/allpass/new int32 id, int32 chans, 
            int32 input, int32 dur, int32 fb, float maxdur;
 */
void arco_allpass_new(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t chans = argv[1]->i;
    int32_t input = argv[2]->i;
    int32_t dur = argv[3]->i;
    int32_t fb = argv[4]->i;
    float maxdur = argv[5]->f;
    // end unpack message

    ANY_UGEN_FROM_ID(input_ugen, input, "arco_allpass_new");
    ANY_UGEN_FROM_ID(dur_ugen, dur, "arco_allpass_new");
    ANY_UGEN_FROM_ID(fb_ugen, fb, "arco_allpass_new");

    new Allpass(id, chans, input_ugen, dur_ugen, fb_ugen, maxdur);
}


/* O2SM INTERFACE: /arco/allpass/repl_input int32 id, int32 input_id;
 */
static void arco_allpass_repl_input(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t input_id = argv[1]->i;
    // end unpack message

    UGEN_FROM_ID(Allpass, allpass, id, "arco_allpass_repl_input");
    ANY_UGEN_FROM_ID(input, input_id, "arco_allpass_repl_input");
    allpass->repl_input(input);
}


/* O2SM INTERFACE: /arco/allpass/max int32 id, float dur;
 */
void arco_allpass_set_max(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    float dur = argv[1]->f;
    // end unpack message

    UGEN_FROM_ID(Allpass, allpass, id, "arco_allpass_set_max");
    allpass->set_max(dur);
}


/* O2SM INTERFACE: /arco/allpass/repl_dur int32 id, int32 dur_id;
 */
static void arco_allpass_repl_dur(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t dur_id = argv[1]->i;
    // end unpack message

    UGEN_FROM_ID(Allpass, allpass, id, "arco_allpass_repl_dur");
    ANY_UGEN_FROM_ID(dur, dur_id, "arco_allpass_repl_dur");
    allpass->repl_dur(dur);
}


/* O2SM INTERFACE: /arco/allpass/set_dur int32 id, int32 chan, float dur;
 */
static void arco_allpass_set_dur(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t chan = argv[1]->i;
    float dur = argv[2]->f;
    // end unpack message

    UGEN_FROM_ID(Allpass, allpass, id, "arco_allpass_repl_dur");
    allpass->set_dur(chan, dur);
}


/* O2SM INTERFACE: /arco/allpass/repl_fb int32 id, int32 fb_id;
 */
static void arco_allpass_repl_fb(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t fb_id = argv[1]->i;
    // end unpack message

    UGEN_FROM_ID(Allpass, allpass, id, "arco_allpass_repl_fb");
    ANY_UGEN_FROM_ID(fb, fb_id, "arco_allpass_repl_fb");
    allpass->repl_fb(fb);
}


/* O2SM INTERFACE: /arco/allpass/set_fb int32 id, int32 chan, float fb;
 */
static void arco_allpass_set_fb(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t chan = argv[1]->i;
    float fb = argv[2]->f;
    // end unpack message

    UGEN_FROM_ID(Allpass, allpass, id, "arco_allpass_repl_fb");
    allpass->set_fb(chan, fb);
}


static void allpass_init()
{
    // O2SM INTERFACE INITIALIZATION: (machine generated)
    o2sm_method_new("/arco/allpass/new", "iiiiif", arco_allpass_new, NULL, true,
                    true);
    o2sm_method_new("/arco/allpass/repl_input", "ii", arco_allpass_repl_input,
                    NULL, true, true);
    o2sm_method_new("/arco/allpass/max", "if", arco_allpass_set_max, NULL, true,
                    true);
    o2sm_method_new("/arco/allpass/repl_dur", "ii", arco_allpass_repl_dur, NULL,
                    true, true);
    o2sm_method_new("/arco/allpass/set_dur", "iif", arco_allpass_set_dur, NULL,
                    true, true);
    o2sm_method_new("/arco/allpass/repl_fb", "ii", arco_allpass_repl_fb, NULL,
                    true, true);
    o2sm_method_new("/arco/allpass/set_fb", "iif", arco_allpass_set_fb, NULL,
                    true, true);
    // END INTERFACE INITIALIZATION
}

Initializer allpass_init_obj(allpass_init);
