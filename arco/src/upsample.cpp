/* upsample -- unit generator for arco
 *
 * Roger B. Dannenberg
 * Jan 2022
 */

#include "arcougen.h"

const char *Upsample_name = "Upsample";

/* O2SM INTERFACE: /arco/new/upsample int32 id, int32 chans, int32 input;
 */
void arco_upsample_new(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t chans = argv[1]->i;
    int32_t input = argv[2]->i;
    // end unpack message

    ANY_UGEN_FROM_ID(ugen, input, "arco_upsample_new");

    new Upsample(id, chans, ugen);
}


/* O2SM INTERFACE: /arco/upsample/repl_input int32 id, int32 input_id;
 */
static void arco_upsample_repl_input(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t input_id = argv[1]->i;
    // end unpack message

    UGEN_FROM_ID(Upsample, upsample, id, "arco_upsample_repl_input");
    ANY_UGEN_FROM_ID(ugen, input_id, "arco_upsample_repl_input");
    upsample->repl_input(ugen);
}


/* O2SM INTERFACE: /arco/upsample/set_input int32 id, int32 chan, float val;
 */
static void arco_upsample_set_input(O2SM_HANDLER_ARGS)
{
    // begin unpack message (machine-generated):
    int32_t id = argv[0]->i;
    int32_t chan = argv[1]->i;
    float val = argv[2]->f;
    // end unpack message

    UGEN_FROM_ID(Upsample, upsample, id, "arco_upsample_set_input");
    upsample->set_input(chan, val);
}


static void upsample_init()
{
    // O2SM INTERFACE INITIALIZATION: (machine generated)
    o2sm_method_new("/arco/new/upsample", "iii", arco_upsample_new, NULL,
                    true, true);
    o2sm_method_new("/arco/upsample/repl_input", "ii",
                    arco_upsample_repl_input, NULL, true, true);
    o2sm_method_new("/arco/upsample/set_input", "iif",
                    arco_upsample_set_input, NULL, true, true);
    // END INTERFACE INITIALIZATION
}

